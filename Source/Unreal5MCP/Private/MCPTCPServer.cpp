// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPTCPServer.h"
#include "MCPCommandHandlers.h"
#include "MCPConstants.h"
#include "Unreal5MCP.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Containers/Ticker.h"
#include "JsonObjectConverter.h"

FMCPTCPServer::FMCPTCPServer(const FMCPTCPServerConfig &InConfig)
    : Config(InConfig), Listener(nullptr), bRunning(false)
{
    // 注册默认命令处理器
    RegisterCommandHandler(MakeShared<FMCPGetSceneInfoHandler>());
    RegisterCommandHandler(MakeShared<FMCPCreateObjectHandler>());
    RegisterCommandHandler(MakeShared<FMCPModifyObjectHandler>());
    RegisterCommandHandler(MakeShared<FMCPDeleteObjectHandler>());
    // Python处理器已移除 - 改用直接UE API操作
}

FMCPTCPServer::~FMCPTCPServer()
{
    Stop();
}

void FMCPTCPServer::RegisterCommandHandler(TSharedPtr<IMCPCommandHandler> Handler)
{
    if (Handler.IsValid())
    {
        FString CommandName = Handler->GetCommandName();
        CommandHandlers.Add(CommandName, Handler);
        MCP_LOG_INFO("Registered command handler: %s", *CommandName);
    }
}

void FMCPTCPServer::UnregisterCommandHandler(const FString &CommandName)
{
    if (CommandHandlers.Remove(CommandName) > 0)
    {
        MCP_LOG_INFO("Unregistered command handler: %s", *CommandName);
    }
}

bool FMCPTCPServer::RegisterExternalCommandHandler(TSharedPtr<IMCPCommandHandler> Handler)
{
    if (!Handler.IsValid())
    {
        MCP_LOG_ERROR("Attempted to register null external command handler");
        return false;
    }

    FString CommandName = Handler->GetCommandName();
    if (CommandHandlers.Contains(CommandName))
    {
        MCP_LOG_WARNING("External command handler '%s' already registered, overwriting", *CommandName);
    }

    CommandHandlers.Add(CommandName, Handler);
    MCP_LOG_INFO("Registered external command handler: %s", *CommandName);
    return true;
}

bool FMCPTCPServer::UnregisterExternalCommandHandler(const FString &CommandName)
{
    if (CommandHandlers.Remove(CommandName) > 0)
    {
        MCP_LOG_INFO("Unregistered external command handler: %s", *CommandName);
        return true;
    }

    MCP_LOG_WARNING("Attempted to unregister non-existent external command handler: %s", *CommandName);
    return false;
}

bool FMCPTCPServer::Start()
{
    if (bRunning)
    {
        MCP_LOG_WARNING("Start called but server is already running");
        return true;
    }

    MCP_LOG_INFO("Starting MCP server on port %d", Config.Port);

    // 创建 TCP 监听器
    Listener = new FTcpListener(FIPv4Endpoint(FIPv4Address::Any, Config.Port));
    if (!Listener || !Listener->IsActive())
    {
        MCP_LOG_ERROR("Failed to start MCP server on port %d", Config.Port);
        Stop();
        return false;
    }

    // 清空现有客户端连接
    ClientConnections.Empty();

    // 注册 Ticker
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FMCPTCPServer::Tick),
        Config.TickIntervalSeconds);

    bRunning = true;
    MCP_LOG_INFO("MCP Server started successfully on port %d", Config.Port);
    return true;
}

void FMCPTCPServer::Stop()
{
    // 清理所有客户端连接
    CleanupAllClientConnections();

    if (Listener)
    {
        delete Listener;
        Listener = nullptr;
    }

    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }

    bRunning = false;
    MCP_LOG_INFO("MCP Server stopped");
}

bool FMCPTCPServer::Tick(float DeltaTime)
{
    if (!bRunning)
        return false;

    // 正常处理
    ProcessPendingConnections();
    ProcessClientData();
    CheckClientTimeouts(DeltaTime);
    return true;
}

void FMCPTCPServer::ProcessPendingConnections()
{
    if (!Listener)
        return;

    // 始终接受新连接
    if (!Listener->OnConnectionAccepted().IsBound())
    {
        Listener->OnConnectionAccepted().BindRaw(this, &FMCPTCPServer::HandleConnectionAccepted);
    }
}

bool FMCPTCPServer::HandleConnectionAccepted(FSocket *InSocket, const FIPv4Endpoint &Endpoint)
{
    if (!InSocket)
    {
        MCP_LOG_ERROR("HandleConnectionAccepted called with null socket");
        return false;
    }

    MCP_LOG_VERBOSE("Connection attempt from %s", *Endpoint.ToString());

    // 接受所有连接
    InSocket->SetNonBlocking(true);

    // 添加到客户端连接列表
    ClientConnections.Add(FMCPClientConnection(InSocket, Endpoint, Config.ReceiveBufferSize));

    MCP_LOG_INFO("MCP Client connected from %s (Total clients: %d)", *Endpoint.ToString(), ClientConnections.Num());
    return true;
}

void FMCPTCPServer::ProcessClientData()
{
    for (int32 i = ClientConnections.Num() - 1; i >= 0; i--)
    {
        FMCPClientConnection &ClientConnection = ClientConnections[i];
        FSocket *ClientSocket = ClientConnection.Socket;

        if (!ClientSocket)
        {
            ClientConnections.RemoveAt(i);
            continue;
        }

        uint32 PendingDataSize = 0;
        if (ClientSocket->HasPendingData(PendingDataSize) && PendingDataSize > 0)
        {
            int32 BytesRead = 0;
            if (ClientSocket->Recv(ClientConnection.ReceiveBuffer.GetData(), ClientConnection.ReceiveBuffer.Num(), BytesRead))
            {
                if (BytesRead > 0)
                {
                    // 重置活动计时器
                    ClientConnection.TimeSinceLastActivity = 0.0f;

                    // 将接收到的数据转换为字符串
                    FString ReceivedData = FString(BytesRead, (ANSICHAR *)ClientConnection.ReceiveBuffer.GetData());

                    MCP_LOG_VERBOSE("Received %d bytes from client %s", BytesRead, *ClientConnection.Endpoint.ToString());

                    // 检查是否是 HTTP 请求
                    if (ReceivedData.StartsWith(TEXT("POST")) || ReceivedData.StartsWith(TEXT("GET")))
                    {
                        // 这是一个 HTTP 请求，提取 JSON body
                        int32 BodyStartIndex = ReceivedData.Find(TEXT("\r\n\r\n"));
                        int32 HeaderLength = 4; // "\r\n\r\n" 的长度

                        if (BodyStartIndex == INDEX_NONE)
                        {
                            BodyStartIndex = ReceivedData.Find(TEXT("\n\n"));
                            HeaderLength = 2; // "\n\n" 的长度
                        }

                        if (BodyStartIndex != INDEX_NONE)
                        {
                            // 跳过分隔符，提取 body（JSON 数据）
                            FString JsonBody = ReceivedData.Mid(BodyStartIndex + HeaderLength).TrimStartAndEnd();

                            MCP_LOG_VERBOSE("Extracted JSON body (%d chars): %s", JsonBody.Len(), *JsonBody.Left(200));

                            if (!JsonBody.IsEmpty())
                            {
                                ProcessCommand(JsonBody, ClientSocket);
                            }
                            else
                            {
                                MCP_LOG_WARNING("Empty JSON body in HTTP request");
                            }
                        }
                        else
                        {
                            MCP_LOG_WARNING("Could not find HTTP header/body separator");
                        }
                    }
                    else
                    {
                        // 纯 JSON 数据（原始 TCP 协议）
                        TArray<FString> Commands;
                        ReceivedData.ParseIntoArray(Commands, TEXT("\n"), true);

                        for (const FString &Command : Commands)
                        {
                            if (!Command.IsEmpty())
                            {
                                ProcessCommand(Command.TrimStartAndEnd(), ClientSocket);
                            }
                        }
                    }
                }
            }
            else
            {
                // 检查是否是真实错误或只是非阻塞 socket 会阻塞
                int32 ErrorCode = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode();
                if (ErrorCode != SE_EWOULDBLOCK)
                {
                    // 真实连接错误，关闭 socket
                    MCP_LOG_WARNING("Socket error %d for client %s, closing connection",
                                    ErrorCode, *ClientConnection.Endpoint.ToString());
                    CleanupClientConnection(ClientConnection);
                }
            }
        }
    }
}

void FMCPTCPServer::ProcessCommand(const FString &CommandJson, FSocket *ClientSocket)
{
    MCP_LOG_VERBOSE("Processing command (%d chars): %s", CommandJson.Len(), *CommandJson.Left(500));

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CommandJson);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        // 检查是否是 JSON-RPC 格式（MCP 协议）
        FString JsonRpcVersion;
        FString Method;
        int32 RequestId = 0;

        bool bIsJsonRpc = JsonObject->TryGetStringField(TEXT("jsonrpc"), JsonRpcVersion) && JsonRpcVersion == TEXT("2.0");
        bool bHasMethod = JsonObject->TryGetStringField(TEXT("method"), Method);
        bool bHasId = JsonObject->TryGetNumberField(TEXT("id"), RequestId);

        if (bIsJsonRpc && bHasMethod)
        {
            // JSON-RPC 请求（MCP 协议）
            MCP_LOG_INFO("Received JSON-RPC method: %s (id: %d)", *Method, RequestId);

            TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
            Response->SetStringField("jsonrpc", TEXT("2.0"));

            if (bHasId)
            {
                Response->SetNumberField("id", RequestId);
            }

            // 处理 MCP 协议方法
            if (Method == TEXT("initialize"))
            {
                // 处理 initialize 请求
                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetStringField("protocolVersion", TEXT("2025-11-13"));

                TSharedPtr<FJsonObject> ServerInfo = MakeShared<FJsonObject>();
                ServerInfo->SetStringField("name", TEXT("Unreal5MCP"));
                ServerInfo->SetStringField("version", TEXT("1.0.0"));
                Result->SetObjectField("serverInfo", ServerInfo);

                TSharedPtr<FJsonObject> Capabilities = MakeShared<FJsonObject>();
                Capabilities->SetBoolField("tools", true);
                Result->SetObjectField("capabilities", Capabilities);

                Response->SetObjectField("result", Result);
                MCP_LOG_INFO("Sent initialize response");
            }
            else if (Method == TEXT("tools/list"))
            {
                // 列出可用工具
                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                TArray<TSharedPtr<FJsonValue>> Tools;

                for (const auto &Pair : CommandHandlers)
                {
                    TSharedPtr<FJsonObject> Tool = MakeShared<FJsonObject>();
                    Tool->SetStringField("name", Pair.Key);
                    Tool->SetStringField("description", FString::Printf(TEXT("Execute %s command"), *Pair.Key));

                    TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
                    InputSchema->SetStringField("type", TEXT("object"));
                    Tool->SetObjectField("inputSchema", InputSchema);

                    Tools.Add(MakeShared<FJsonValueObject>(Tool));
                }

                Result->SetArrayField("tools", Tools);
                Response->SetObjectField("result", Result);
                MCP_LOG_INFO("Sent tools/list response with %d tools", Tools.Num());
            }
            else if (Method == TEXT("tools/call"))
            {
                // 调用工具
                const TSharedPtr<FJsonObject> *ParamsObj;
                if (JsonObject->TryGetObjectField(TEXT("params"), ParamsObj))
                {
                    FString ToolName;
                    if ((*ParamsObj)->TryGetStringField(TEXT("name"), ToolName))
                    {
                        const TSharedPtr<FJsonObject> *ToolArgs;
                        (*ParamsObj)->TryGetObjectField(TEXT("arguments"), ToolArgs);

                        if (TSharedPtr<IMCPCommandHandler> *HandlerPtr = CommandHandlers.Find(ToolName))
                        {
                            MCP_LOG_INFO("Executing tool: %s", *ToolName);
                            TSharedPtr<FJsonObject> ToolResult = (*HandlerPtr)->Execute(ToolArgs ? *ToolArgs : *ParamsObj, ClientSocket);
                            Response->SetObjectField("result", ToolResult);
                        }
                        else
                        {
                            TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
                            Error->SetNumberField("code", -32601);
                            Error->SetStringField("message", TEXT("Tool not found"));
                            Response->SetObjectField("error", Error);
                        }
                    }
                }
            }
            else
            {
                // 未知方法
                TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
                Error->SetNumberField("code", -32601);
                Error->SetStringField("message", TEXT("Method not found"));
                Response->SetObjectField("error", Error);
                MCP_LOG_WARNING("Unknown JSON-RPC method: %s", *Method);
            }

            SendResponse(ClientSocket, Response);
        }
        else
        {
            // 尝试简单命令格式（向后兼容）
            FString CommandType;
            if (JsonObject->TryGetStringField(TEXT("type"), CommandType))
            {
                if (TSharedPtr<IMCPCommandHandler> *HandlerPtr = CommandHandlers.Find(CommandType))
                {
                    TSharedPtr<IMCPCommandHandler> Handler = *HandlerPtr;
                    MCP_LOG_INFO("Executing command: %s", *CommandType);
                    TSharedPtr<FJsonObject> Response = Handler->Execute(JsonObject, ClientSocket);
                    if (Response.IsValid())
                    {
                        SendResponse(ClientSocket, Response);
                    }
                }
                else
                {
                    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
                    Response->SetStringField("status", TEXT("error"));
                    Response->SetStringField("message", FString::Printf(TEXT("Unknown command type: %s"), *CommandType));
                    SendResponse(ClientSocket, Response);
                }
            }
            else
            {
                MCP_LOG_WARNING("Invalid request format. JSON keys: ");
                TArray<FString> Keys;
                JsonObject->Values.GetKeys(Keys);
                for (const FString &Key : Keys)
                {
                    MCP_LOG_WARNING("  - %s", *Key);
                }
            }
        }
    }
    else
    {
        MCP_LOG_WARNING("Invalid JSON format (first 200 chars): %s", *CommandJson.Left(200));
    }
}

void FMCPTCPServer::SendResponse(FSocket *Client, const TSharedPtr<FJsonObject> &Response)
{
    if (!Client || !Response.IsValid())
    {
        return;
    }

    // 将响应转换为 JSON 字符串
    FString ResponseString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);

    // 构建 HTTP 响应
    FString HttpResponse = TEXT("HTTP/1.1 200 OK\r\n");
    HttpResponse += TEXT("Content-Type: application/json\r\n");
    HttpResponse += TEXT("Access-Control-Allow-Origin: *\r\n");
    HttpResponse += TEXT("Connection: close\r\n");

    // 计算内容长度
    FTCHARToUTF8 JsonConverter(*ResponseString);
    int32 ContentLength = JsonConverter.Length();
    HttpResponse += FString::Printf(TEXT("Content-Length: %d\r\n"), ContentLength);
    HttpResponse += TEXT("\r\n");
    HttpResponse += ResponseString;

    // 发送响应
    int32 BytesSent = 0;
    FTCHARToUTF8 Converter(*HttpResponse);
    if (!Client->Send((const uint8 *)Converter.Get(), Converter.Length(), BytesSent))
    {
        MCP_LOG_ERROR("Failed to send response to client");
    }
    else
    {
        MCP_LOG_VERBOSE("Sent %d bytes HTTP response to client", BytesSent);
    }
}

void FMCPTCPServer::CheckClientTimeouts(float DeltaTime)
{
    for (int32 i = ClientConnections.Num() - 1; i >= 0; i--)
    {
        FMCPClientConnection &ClientConnection = ClientConnections[i];
        ClientConnection.TimeSinceLastActivity += DeltaTime;

        if (ClientConnection.TimeSinceLastActivity > Config.ClientTimeoutSeconds)
        {
            MCP_LOG_WARNING("Client %s timed out after %.1f seconds",
                            *ClientConnection.Endpoint.ToString(),
                            ClientConnection.TimeSinceLastActivity);
            CleanupClientConnection(ClientConnection);
        }
    }
}

void FMCPTCPServer::CleanupAllClientConnections()
{
    MCP_LOG_INFO("Cleaning up all client connections (%d clients)", ClientConnections.Num());

    for (FMCPClientConnection &ClientConnection : ClientConnections)
    {
        if (ClientConnection.Socket)
        {
            ClientConnection.Socket->Close();
            ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientConnection.Socket);
            ClientConnection.Socket = nullptr;
        }
    }

    ClientConnections.Empty();
}

void FMCPTCPServer::CleanupClientConnection(FSocket *ClientSocket)
{
    for (int32 i = 0; i < ClientConnections.Num(); i++)
    {
        if (ClientConnections[i].Socket == ClientSocket)
        {
            CleanupClientConnection(ClientConnections[i]);
            break;
        }
    }
}

void FMCPTCPServer::CleanupClientConnection(FMCPClientConnection &ClientConnection)
{
    if (ClientConnection.Socket)
    {
        MCP_LOG_INFO("Cleaning up client connection from %s", *ClientConnection.Endpoint.ToString());

        ClientConnection.Socket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientConnection.Socket);
        ClientConnection.Socket = nullptr;
    }

    // 从列表中移除
    ClientConnections.RemoveAll([&ClientConnection](const FMCPClientConnection &Connection)
                                { return Connection.Socket == ClientConnection.Socket; });
}

FString FMCPTCPServer::GetSafeSocketDescription(FSocket *Socket)
{
    if (!Socket)
    {
        return TEXT("null");
    }

    TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    if (Socket->GetPeerAddress(*RemoteAddr))
    {
        return RemoteAddr->ToString(true);
    }

    return TEXT("unknown");
}
