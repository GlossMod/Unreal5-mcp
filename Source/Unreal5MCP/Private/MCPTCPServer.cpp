// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPTCPServer.h"
#include "MCPCommandHandlers.h"
#include "MCPConstants.h"
#include "MCPSettings.h"
#include "Unreal5MCP.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Containers/Ticker.h"
#include "Json.h"
#include "JsonObjectConverter.h"

FMCPTCPServer::FMCPTCPServer(const FMCPTCPServerConfig &InConfig)
    : Config(InConfig), Listener(nullptr), bRunning(false)
{
    // ============================================================================
    // 注册基础命令处理器
    // ============================================================================
    RegisterCommandHandler(MakeShared<FMCPGetSceneInfoHandler>());
    RegisterCommandHandler(MakeShared<FMCPCreateObjectHandler>());
    RegisterCommandHandler(MakeShared<FMCPModifyObjectHandler>());
    RegisterCommandHandler(MakeShared<FMCPDeleteObjectHandler>());

    // ============================================================================
    // 注册蓝图命令处理器
    // ============================================================================
    RegisterCommandHandler(MakeShared<FMCPCreateBlueprintHandler>());
    RegisterCommandHandler(MakeShared<FMCPGetBlueprintInfoHandler>());
    RegisterCommandHandler(MakeShared<FMCPModifyBlueprintHandler>());
    RegisterCommandHandler(MakeShared<FMCPCompileBlueprintHandler>());

    // ============================================================================
    // 注册场景编辑命令处理器
    // ============================================================================
    RegisterCommandHandler(MakeShared<FMCPSetCameraHandler>());
    RegisterCommandHandler(MakeShared<FMCPGetCameraHandler>());
    RegisterCommandHandler(MakeShared<FMCPCreateLightHandler>());
    RegisterCommandHandler(MakeShared<FMCPSelectActorHandler>());
    RegisterCommandHandler(MakeShared<FMCPGetSelectedActorsHandler>());

    // ============================================================================
    // 注册资源管理命令处理器
    // ============================================================================
    RegisterCommandHandler(MakeShared<FMCPImportAssetHandler>());
    RegisterCommandHandler(MakeShared<FMCPCreateMaterialHandler>());
    RegisterCommandHandler(MakeShared<FMCPListAssetsHandler>());

    // ============================================================================
    // 注册批量操作命令处理器
    // ============================================================================
    RegisterCommandHandler(MakeShared<FMCPBatchCreateHandler>());
    RegisterCommandHandler(MakeShared<FMCPBatchModifyHandler>());
    RegisterCommandHandler(MakeShared<FMCPBatchDeleteHandler>());

    MCP_LOG_INFO("MCP Server initialized with %d command handlers", CommandHandlers.Num());
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

                // 定义每个工具的详细 schema
                TMap<FString, TFunction<TSharedPtr<FJsonObject>()>> SchemaGenerators;

                // batch_create schema
                SchemaGenerators.Add(TEXT("batch_create"), []() -> TSharedPtr<FJsonObject>
                                     {
                    TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
                    Schema->SetStringField("type", TEXT("object"));
                    
                    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
                    
                    // actors 数组
                    TSharedPtr<FJsonObject> ActorsSchema = MakeShared<FJsonObject>();
                    ActorsSchema->SetStringField("type", TEXT("array"));
                    ActorsSchema->SetStringField("description", TEXT("Array of actors to create"));
                    
                    TSharedPtr<FJsonObject> ActorItemSchema = MakeShared<FJsonObject>();
                    ActorItemSchema->SetStringField("type", TEXT("object"));
                    
                    TSharedPtr<FJsonObject> ActorProps = MakeShared<FJsonObject>();
                    
                    // class_name
                    TSharedPtr<FJsonObject> ClassNameSchema = MakeShared<FJsonObject>();
                    ClassNameSchema->SetStringField("type", TEXT("string"));
                    ClassNameSchema->SetStringField("description", TEXT("Full UE class name (e.g., 'ASkyAtmosphere', 'ASkyLight', 'AStaticMeshActor')"));
                    ActorProps->SetObjectField("class_name", ClassNameSchema);
                    
                    // name
                    TSharedPtr<FJsonObject> NameSchema = MakeShared<FJsonObject>();
                    NameSchema->SetStringField("type", TEXT("string"));
                    NameSchema->SetStringField("description", TEXT("Actor name"));
                    ActorProps->SetObjectField("name", NameSchema);
                    
                    // location
                    TSharedPtr<FJsonObject> LocationSchema = MakeShared<FJsonObject>();
                    LocationSchema->SetStringField("type", TEXT("object"));
                    LocationSchema->SetStringField("description", TEXT("Actor location as {x, y, z}"));
                    TSharedPtr<FJsonObject> LocationProps = MakeShared<FJsonObject>();
                    TSharedPtr<FJsonObject> XSchema = MakeShared<FJsonObject>();
                    XSchema->SetStringField("type", TEXT("number"));
                    LocationProps->SetObjectField("x", XSchema);
                    TSharedPtr<FJsonObject> YSchema = MakeShared<FJsonObject>();
                    YSchema->SetStringField("type", TEXT("number"));
                    LocationProps->SetObjectField("y", YSchema);
                    TSharedPtr<FJsonObject> ZSchema = MakeShared<FJsonObject>();
                    ZSchema->SetStringField("type", TEXT("number"));
                    LocationProps->SetObjectField("z", ZSchema);
                    LocationSchema->SetObjectField("properties", LocationProps);
                    ActorProps->SetObjectField("location", LocationSchema);
                    
                    // rotation (optional)
                    TSharedPtr<FJsonObject> RotationSchema = MakeShared<FJsonObject>();
                    RotationSchema->SetStringField("type", TEXT("object"));
                    RotationSchema->SetStringField("description", TEXT("Actor rotation as {pitch, yaw, roll} (optional)"));
                    ActorProps->SetObjectField("rotation", RotationSchema);
                    
                    // scale (optional)
                    TSharedPtr<FJsonObject> ScaleSchema = MakeShared<FJsonObject>();
                    ScaleSchema->SetStringField("type", TEXT("object"));
                    ScaleSchema->SetStringField("description", TEXT("Actor scale as {x, y, z} (optional)"));
                    ActorProps->SetObjectField("scale", ScaleSchema);
                    
                    ActorItemSchema->SetObjectField("properties", ActorProps);
                    
                    TArray<TSharedPtr<FJsonValue>> RequiredFields;
                    RequiredFields.Add(MakeShared<FJsonValueString>(TEXT("class_name")));
                    ActorItemSchema->SetArrayField("required", RequiredFields);
                    
                    ActorsSchema->SetObjectField("items", ActorItemSchema);
                    Props->SetObjectField("actors", ActorsSchema);
                    
                    Schema->SetObjectField("properties", Props);
                    
                    TArray<TSharedPtr<FJsonValue>> RequiredTop;
                    RequiredTop.Add(MakeShared<FJsonValueString>(TEXT("actors")));
                    Schema->SetArrayField("required", RequiredTop);
                    
                    return Schema; });

                // create_object schema
                SchemaGenerators.Add(TEXT("create_object"), []() -> TSharedPtr<FJsonObject>
                                     {
                    TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
                    Schema->SetStringField("type", TEXT("object"));
                    
                    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
                    
                    TSharedPtr<FJsonObject> ClassNameSchema = MakeShared<FJsonObject>();
                    ClassNameSchema->SetStringField("type", TEXT("string"));
                    ClassNameSchema->SetStringField("description", TEXT("Full UE class name (e.g., 'ASkyAtmosphere', 'AStaticMeshActor')"));
                    Props->SetObjectField("class_name", ClassNameSchema);
                    
                    TSharedPtr<FJsonObject> NameSchema = MakeShared<FJsonObject>();
                    NameSchema->SetStringField("type", TEXT("string"));
                    NameSchema->SetStringField("description", TEXT("Actor name (optional)"));
                    Props->SetObjectField("name", NameSchema);
                    
                    TSharedPtr<FJsonObject> LocationSchema = MakeShared<FJsonObject>();
                    LocationSchema->SetStringField("type", TEXT("object"));
                    LocationSchema->SetStringField("description", TEXT("Location as {x, y, z} (optional)"));
                    Props->SetObjectField("location", LocationSchema);
                    
                    TSharedPtr<FJsonObject> RotationSchema = MakeShared<FJsonObject>();
                    RotationSchema->SetStringField("type", TEXT("object"));
                    RotationSchema->SetStringField("description", TEXT("Rotation as {pitch, yaw, roll} (optional)"));
                    Props->SetObjectField("rotation", RotationSchema);
                    
                    TSharedPtr<FJsonObject> ScaleSchema = MakeShared<FJsonObject>();
                    ScaleSchema->SetStringField("type", TEXT("object"));
                    ScaleSchema->SetStringField("description", TEXT("Scale as {x, y, z} (optional)"));
                    Props->SetObjectField("scale", ScaleSchema);
                    
                    TSharedPtr<FJsonObject> AssetPathSchema = MakeShared<FJsonObject>();
                    AssetPathSchema->SetStringField("type", TEXT("string"));
                    AssetPathSchema->SetStringField("description", TEXT("Asset path for StaticMeshActor (optional)"));
                    Props->SetObjectField("asset_path", AssetPathSchema);
                    
                    Schema->SetObjectField("properties", Props);
                    
                    TArray<TSharedPtr<FJsonValue>> Required;
                    Required.Add(MakeShared<FJsonValueString>(TEXT("class_name")));
                    Schema->SetArrayField("required", Required);
                    
                    return Schema; });

                // create_blueprint schema
                SchemaGenerators.Add(TEXT("create_blueprint"), []() -> TSharedPtr<FJsonObject>
                                     {
                    TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
                    Schema->SetStringField("type", TEXT("object"));
                    
                    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
                    
                    TSharedPtr<FJsonObject> PathSchema = MakeShared<FJsonObject>();
                    PathSchema->SetStringField("type", TEXT("string"));
                    PathSchema->SetStringField("description", TEXT("Blueprint package path (e.g., '/Game/Blueprints/BP_MyActor')"));
                    Props->SetObjectField("path", PathSchema);
                    
                    TSharedPtr<FJsonObject> NameSchema = MakeShared<FJsonObject>();
                    NameSchema->SetStringField("type", TEXT("string"));
                    NameSchema->SetStringField("description", TEXT("Blueprint name (optional, extracted from path if not provided)"));
                    Props->SetObjectField("name", NameSchema);
                    
                    TSharedPtr<FJsonObject> ParentClassSchema = MakeShared<FJsonObject>();
                    ParentClassSchema->SetStringField("type", TEXT("string"));
                    ParentClassSchema->SetStringField("description", TEXT("Parent class name (e.g., 'Actor', 'Character', 'Pawn'). Default: 'Character'"));
                    Props->SetObjectField("parent_class", ParentClassSchema);
                    
                    Schema->SetObjectField("properties", Props);
                    
                    TArray<TSharedPtr<FJsonValue>> Required;
                    Required.Add(MakeShared<FJsonValueString>(TEXT("path")));
                    Schema->SetArrayField("required", Required);
                    
                    return Schema; });

                // get_blueprint_info schema
                SchemaGenerators.Add(TEXT("get_blueprint_info"), []() -> TSharedPtr<FJsonObject>
                                     {
                    TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
                    Schema->SetStringField("type", TEXT("object"));
                    
                    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
                    
                    TSharedPtr<FJsonObject> PathSchema = MakeShared<FJsonObject>();
                    PathSchema->SetStringField("type", TEXT("string"));
                    PathSchema->SetStringField("description", TEXT("Blueprint asset path"));
                    Props->SetObjectField("path", PathSchema);
                    
                    Schema->SetObjectField("properties", Props);
                    
                    TArray<TSharedPtr<FJsonValue>> Required;
                    Required.Add(MakeShared<FJsonValueString>(TEXT("path")));
                    Schema->SetArrayField("required", Required);
                    
                    return Schema; });

                // modify_blueprint schema
                SchemaGenerators.Add(TEXT("modify_blueprint"), []() -> TSharedPtr<FJsonObject>
                                     {
                    TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
                    Schema->SetStringField("type", TEXT("object"));
                    
                    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
                    
                    TSharedPtr<FJsonObject> PathSchema = MakeShared<FJsonObject>();
                    PathSchema->SetStringField("type", TEXT("string"));
                    PathSchema->SetStringField("description", TEXT("Blueprint asset path"));
                    Props->SetObjectField("path", PathSchema);
                    
                    TSharedPtr<FJsonObject> DescSchema = MakeShared<FJsonObject>();
                    DescSchema->SetStringField("type", TEXT("string"));
                    DescSchema->SetStringField("description", TEXT("Blueprint description (optional)"));
                    Props->SetObjectField("description", DescSchema);
                    
                    Schema->SetObjectField("properties", Props);
                    
                    TArray<TSharedPtr<FJsonValue>> Required;
                    Required.Add(MakeShared<FJsonValueString>(TEXT("path")));
                    Schema->SetArrayField("required", Required);
                    
                    return Schema; });

                // compile_blueprint schema
                SchemaGenerators.Add(TEXT("compile_blueprint"), []() -> TSharedPtr<FJsonObject>
                                     {
                    TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
                    Schema->SetStringField("type", TEXT("object"));
                    
                    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
                    
                    TSharedPtr<FJsonObject> PathSchema = MakeShared<FJsonObject>();
                    PathSchema->SetStringField("type", TEXT("string"));
                    PathSchema->SetStringField("description", TEXT("Blueprint asset path"));
                    Props->SetObjectField("path", PathSchema);
                    
                    Schema->SetObjectField("properties", Props);
                    
                    TArray<TSharedPtr<FJsonValue>> Required;
                    Required.Add(MakeShared<FJsonValueString>(TEXT("path")));
                    Schema->SetArrayField("required", Required);
                    
                    return Schema; });

                // 为每个命令生成工具定义
                for (const auto &Pair : CommandHandlers)
                {
                    TSharedPtr<FJsonObject> Tool = MakeShared<FJsonObject>();
                    Tool->SetStringField("name", Pair.Key);

                    // 为已知命令提供详细描述
                    if (Pair.Key == TEXT("batch_create"))
                    {
                        Tool->SetStringField("description", TEXT("Batch create multiple actors in the scene. Requires 'actors' array with each actor having 'class_name' (required), 'name', 'location' ({x,y,z}), 'rotation' ({pitch,yaw,roll}), and 'scale' ({x,y,z})."));
                    }
                    else if (Pair.Key == TEXT("create_object"))
                    {
                        Tool->SetStringField("description", TEXT("Create a single actor in the scene. Requires 'class_name' (e.g., 'ASkyAtmosphere', 'ASkyLight', 'AStaticMeshActor'). Optional: 'name', 'location' ({x,y,z}), 'rotation' ({pitch,yaw,roll}), 'scale' ({x,y,z}), 'asset_path'."));
                    }
                    else if (Pair.Key == TEXT("modify_object"))
                    {
                        Tool->SetStringField("description", TEXT("Modify an existing actor. Requires 'actor_name'. Can update 'location' ({x,y,z}), 'rotation' ({pitch,yaw,roll}), 'scale' ({x,y,z}), and other properties."));
                    }
                    else if (Pair.Key == TEXT("batch_modify"))
                    {
                        Tool->SetStringField("description", TEXT("Batch modify multiple actors. Requires 'actors' array with each containing 'name' and properties to modify."));
                    }
                    else if (Pair.Key == TEXT("batch_delete"))
                    {
                        Tool->SetStringField("description", TEXT("Batch delete multiple actors. Requires 'actor_names' array of actor names to delete."));
                    }
                    else if (Pair.Key == TEXT("create_blueprint"))
                    {
                        Tool->SetStringField("description", TEXT("Create a new Blueprint class. Requires 'path' (package path). Optional: 'name', 'parent_class' (default: 'Character'). Note: Does not support 'components' parameter - components must be added after creation."));
                    }
                    else if (Pair.Key == TEXT("get_blueprint_info"))
                    {
                        Tool->SetStringField("description", TEXT("Get information about a Blueprint. Requires 'path' (asset path)."));
                    }
                    else if (Pair.Key == TEXT("modify_blueprint"))
                    {
                        Tool->SetStringField("description", TEXT("Modify a Blueprint. Requires 'path'. Optional: 'description'."));
                    }
                    else if (Pair.Key == TEXT("compile_blueprint"))
                    {
                        Tool->SetStringField("description", TEXT("Compile a Blueprint. Requires 'path' (asset path)."));
                    }
                    else if (Pair.Key == TEXT("get_scene_info"))
                    {
                        Tool->SetStringField("description", TEXT("Get information about all actors in the current scene."));
                    }
                    else if (Pair.Key == TEXT("delete_object"))
                    {
                        Tool->SetStringField("description", TEXT("Delete an actor from the scene. Requires 'actor_name'."));
                    }
                    else if (Pair.Key == TEXT("create_light"))
                    {
                        Tool->SetStringField("description", TEXT("Create a light actor. Specify light type and properties."));
                    }
                    else if (Pair.Key == TEXT("create_material"))
                    {
                        Tool->SetStringField("description", TEXT("Create a material asset."));
                    }
                    else if (Pair.Key == TEXT("import_asset"))
                    {
                        Tool->SetStringField("description", TEXT("Import an external asset file."));
                    }
                    else if (Pair.Key == TEXT("list_assets"))
                    {
                        Tool->SetStringField("description", TEXT("List assets in a directory."));
                    }
                    else if (Pair.Key == TEXT("set_camera"))
                    {
                        Tool->SetStringField("description", TEXT("Set editor camera position and rotation."));
                    }
                    else if (Pair.Key == TEXT("get_camera"))
                    {
                        Tool->SetStringField("description", TEXT("Get current editor camera position and rotation."));
                    }
                    else if (Pair.Key == TEXT("select_actor"))
                    {
                        Tool->SetStringField("description", TEXT("Select an actor in the editor."));
                    }
                    else if (Pair.Key == TEXT("get_selected_actors"))
                    {
                        Tool->SetStringField("description", TEXT("Get list of currently selected actors."));
                    }
                    else
                    {
                        Tool->SetStringField("description", FString::Printf(TEXT("Execute %s command"), *Pair.Key));
                    }

                    // 使用详细的 schema 或默认 schema
                    if (auto SchemaGen = SchemaGenerators.Find(Pair.Key))
                    {
                        Tool->SetObjectField("inputSchema", (*SchemaGen)());
                    }
                    else
                    {
                        TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
                        InputSchema->SetStringField("type", TEXT("object"));
                        Tool->SetObjectField("inputSchema", InputSchema);
                    }

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

                            // 根据 MCP 标准, tools/call 的响应必须包含 content 数组
                            if (ToolResult.IsValid())
                            {
                                TSharedPtr<FJsonObject> ResultWrapper = MakeShared<FJsonObject>();

                                // 创建 content 数组
                                TArray<TSharedPtr<FJsonValue>> ContentArray;
                                TSharedPtr<FJsonObject> ContentItem = MakeShared<FJsonObject>();
                                ContentItem->SetStringField("type", TEXT("text"));

                                // 将工具结果转换为 JSON 字符串
                                FString ResultStr;
                                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
                                FJsonSerializer::Serialize(ToolResult.ToSharedRef(), Writer);
                                ContentItem->SetStringField("text", ResultStr);

                                ContentArray.Add(MakeShared<FJsonValueObject>(ContentItem));
                                ResultWrapper->SetArrayField("content", ContentArray);
                                Response->SetObjectField("result", ResultWrapper);
                            }
                            else
                            {
                                TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
                                Error->SetNumberField("code", -32603);
                                Error->SetStringField("message", TEXT("Internal error: Tool returned null result"));
                                Response->SetObjectField("error", Error);
                            }
                        }
                        else
                        {
                            TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
                            Error->SetNumberField("code", -32601);
                            Error->SetStringField("message", TEXT("Tool not found"));
                            Response->SetObjectField("error", Error);
                        }
                    }
                    else
                    {
                        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
                        Error->SetNumberField("code", -32602);
                        Error->SetStringField("message", TEXT("Missing 'name' parameter"));
                        Response->SetObjectField("error", Error);
                    }
                }
                else
                {
                    TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
                    Error->SetNumberField("code", -32602);
                    Error->SetStringField("message", TEXT("Missing 'params' object"));
                    Response->SetObjectField("error", Error);
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

// ============================================================================
// 配置结构实现
// ============================================================================

FMCPTCPServerConfig FMCPTCPServerConfig::FromSettings(const class UMCPSettings *Settings)
{
    FMCPTCPServerConfig Config;

    if (Settings)
    {
        Config.Port = Settings->Port;
        Config.ClientTimeoutSeconds = Settings->ClientTimeoutSeconds;
        Config.MaxConcurrentClients = Settings->MaxConcurrentClients;
        Config.bLocalhostOnly = Settings->bLocalhostOnly;
        Config.bEnableVerboseLogging = Settings->bEnableVerboseLogging;
        Config.bLogFullJsonMessages = Settings->bLogFullJsonMessages;
        Config.TickIntervalSeconds = Settings->ServerTickInterval;
        Config.MaxActorsInSceneInfo = Settings->MaxActorsInSceneInfo;
        Config.CommandExecutionTimeout = Settings->CommandExecutionTimeout;
    }

    return Config;
}

bool FMCPTCPServerConfig::Validate(FString &OutErrorMessage) const
{
    // 验证端口
    if (!MCPConstants::IsValidPort(Port))
    {
        OutErrorMessage = FString::Printf(TEXT("Invalid port %d. Must be between %d and %d."),
                                          Port, MCPConstants::MIN_PORT, MCPConstants::MAX_PORT);
        return false;
    }

    // 验证超时
    if (!MCPConstants::IsValidTimeout(ClientTimeoutSeconds))
    {
        OutErrorMessage = FString::Printf(TEXT("Invalid client timeout %.1f. Must be between %.1f and %.1f seconds."),
                                          ClientTimeoutSeconds, MCPConstants::MIN_CLIENT_TIMEOUT_SECONDS,
                                          MCPConstants::MAX_CLIENT_TIMEOUT_SECONDS);
        return false;
    }

    // 验证最大客户端数
    if (MaxConcurrentClients < 1 || MaxConcurrentClients > 50)
    {
        OutErrorMessage = FString::Printf(TEXT("Invalid max concurrent clients %d. Must be between 1 and 50."),
                                          MaxConcurrentClients);
        return false;
    }

    OutErrorMessage.Empty();
    return true;
}
