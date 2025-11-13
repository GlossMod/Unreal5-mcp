#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Json.h"
#include "Networking.h"
#include "Common/TcpListener.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "MCPConstants.h"

/**
 * TCP 服务器配置结构
 */
struct FMCPTCPServerConfig
{
    /** 监听端口 */
    int32 Port = MCPConstants::DEFAULT_PORT;

    /** 客户端超时时间（秒） */
    float ClientTimeoutSeconds = MCPConstants::DEFAULT_CLIENT_TIMEOUT_SECONDS;

    /** 接收缓冲区大小（字节） */
    int32 ReceiveBufferSize = MCPConstants::DEFAULT_RECEIVE_BUFFER_SIZE;

    /** Tick 间隔（秒） */
    float TickIntervalSeconds = MCPConstants::DEFAULT_TICK_INTERVAL_SECONDS;

    /** 是否启用详细日志 */
    bool bEnableVerboseLogging = MCPConstants::DEFAULT_VERBOSE_LOGGING;
};

/**
 * 客户端连接信息结构
 */
struct FMCPClientConnection
{
    /** 客户端 Socket */
    FSocket *Socket;

    /** 端点信息 */
    FIPv4Endpoint Endpoint;

    /** 上次活动时间（用于超时跟踪） */
    float TimeSinceLastActivity;

    /** 接收缓冲区 */
    TArray<uint8> ReceiveBuffer;

    /**
     * 构造函数
     */
    FMCPClientConnection(FSocket *InSocket, const FIPv4Endpoint &InEndpoint, int32 BufferSize = MCPConstants::DEFAULT_RECEIVE_BUFFER_SIZE)
        : Socket(InSocket), Endpoint(InEndpoint), TimeSinceLastActivity(0.0f)
    {
        ReceiveBuffer.SetNumUninitialized(BufferSize);
    }
};

/**
 * 命令处理器接口
 * 允许轻松添加新命令而无需修改服务器
 */
class IMCPCommandHandler
{
public:
    virtual ~IMCPCommandHandler() {}

    /**
     * 获取此处理器响应的命令名称
     */
    virtual FString GetCommandName() const = 0;

    /**
     * 执行命令
     * @param Params - 命令参数
     * @param ClientSocket - 客户端 Socket
     * @return JSON 响应对象
     */
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) = 0;
};

/**
 * MCP TCP 服务器
 * 管理连接和命令路由
 */
class UNREAL5MCP_API FMCPTCPServer
{
public:
    /**
     * 构造函数
     */
    FMCPTCPServer(const FMCPTCPServerConfig &InConfig);

    /**
     * 析构函数
     */
    virtual ~FMCPTCPServer();

    /**
     * 启动服务器
     */
    bool Start();

    /**
     * 停止服务器
     */
    void Stop();

    /**
     * 检查服务器是否正在运行
     */
    bool IsRunning() const { return bRunning; }

    /**
     * 注册命令处理器
     */
    void RegisterCommandHandler(TSharedPtr<IMCPCommandHandler> Handler);

    /**
     * 注销命令处理器
     */
    void UnregisterCommandHandler(const FString &CommandName);

    /**
     * 注册外部命令处理器
     */
    bool RegisterExternalCommandHandler(TSharedPtr<IMCPCommandHandler> Handler);

    /**
     * 注销外部命令处理器
     */
    bool UnregisterExternalCommandHandler(const FString &CommandName);

    /**
     * 发送响应到客户端
     */
    void SendResponse(FSocket *Client, const TSharedPtr<FJsonObject> &Response);

    /**
     * 获取命令处理器映射（用于测试）
     */
    const TMap<FString, TSharedPtr<IMCPCommandHandler>> &GetCommandHandlers() const { return CommandHandlers; }

protected:
    /**
     * Ticker 调用的 Tick 函数
     */
    bool Tick(float DeltaTime);

    /**
     * 处理待处理的连接
     */
    virtual void ProcessPendingConnections();

    /**
     * 处理客户端数据
     */
    virtual void ProcessClientData();

    /**
     * 处理命令
     */
    virtual void ProcessCommand(const FString &CommandJson, FSocket *ClientSocket);

    /**
     * 检查客户端超时
     */
    virtual void CheckClientTimeouts(float DeltaTime);

    /**
     * 清理客户端连接
     */
    virtual void CleanupClientConnection(FMCPClientConnection &ClientConnection);
    virtual void CleanupClientConnection(FSocket *ClientSocket);
    virtual void CleanupAllClientConnections();

    /**
     * 获取 Socket 的安全描述
     */
    FString GetSafeSocketDescription(FSocket *Socket);

    /**
     * 连接处理器
     */
    virtual bool HandleConnectionAccepted(FSocket *InSocket, const FIPv4Endpoint &Endpoint);

    /** 服务器配置 */
    FMCPTCPServerConfig Config;

    /** TCP 监听器 */
    FTcpListener *Listener;

    /** 客户端连接列表 */
    TArray<FMCPClientConnection> ClientConnections;

    /** 运行标志 */
    bool bRunning;

    /** Ticker 句柄 */
    FTSTicker::FDelegateHandle TickerHandle;

    /** 命令处理器映射 */
    TMap<FString, TSharedPtr<IMCPCommandHandler>> CommandHandlers;

private:
    // 禁用拷贝和赋值
    FMCPTCPServer(const FMCPTCPServer &) = delete;
    FMCPTCPServer &operator=(const FMCPTCPServer &) = delete;
};
