#pragma once

#include "CoreMinimal.h"

/**
 * MCPConstants - MCP 插件全局常量定义
 *
 * 此命名空间包含MCP插件使用的所有常量配置值，包括:
 * - 网络配置: 端口、缓冲区大小、超时设置
 * - 协议配置: MCP协议版本、消息格式
 * - 性能配置: 限制和阈值
 * - 安全配置: 访问控制和验证
 * - 路径配置: 运行时初始化的路径常量
 */
namespace MCPConstants
{
    // ============================================================================
    // 网络配置常量
    // ============================================================================

    /** 默认TCP监听端口 (13377) */
    constexpr int32 DEFAULT_PORT = 13377;

    /** 最小允许端口号 */
    constexpr int32 MIN_PORT = 1024;

    /** 最大允许端口号 */
    constexpr int32 MAX_PORT = 65535;

    /** 接收缓冲区大小 (64KB) - 用于接收客户端数据 */
    constexpr int32 DEFAULT_RECEIVE_BUFFER_SIZE = 65536;

    /** 发送缓冲区大小 (64KB) - 用于发送响应数据 */
    constexpr int32 DEFAULT_SEND_BUFFER_SIZE = DEFAULT_RECEIVE_BUFFER_SIZE;

    /** 最大消息大小 (1MB) - 防止内存溢出 */
    constexpr int32 MAX_MESSAGE_SIZE = 1048576;

    /** 客户端超时时间 (秒) - 无活动后断开连接 */
    constexpr float DEFAULT_CLIENT_TIMEOUT_SECONDS = 30.0f;

    /** 最小客户端超时时间 (秒) */
    constexpr float MIN_CLIENT_TIMEOUT_SECONDS = 5.0f;

    /** 最大客户端超时时间 (秒) */
    constexpr float MAX_CLIENT_TIMEOUT_SECONDS = 300.0f;

    /** 服务器Tick间隔 (秒) - 处理连接和数据的频率 */
    constexpr float DEFAULT_TICK_INTERVAL_SECONDS = 0.1f;

    /** 最大同时连接的客户端数量 */
    constexpr int32 MAX_CONCURRENT_CLIENTS = 10;

    /** 连接队列大小 - 待处理连接的最大数量 */
    constexpr int32 CONNECTION_QUEUE_SIZE = 5;

    // ============================================================================
    // 协议常量
    // ============================================================================

    /** MCP 协议版本 */
    static const FString MCP_PROTOCOL_VERSION = TEXT("2025-11-13");

    /** 服务器名称 */
    static const FString SERVER_NAME = TEXT("Unreal5MCP");

    /** 服务器版本 */
    static const FString SERVER_VERSION = TEXT("1.0.0");

    /** JSON-RPC 版本 */
    static const FString JSONRPC_VERSION = TEXT("2.0");

    /** HTTP 响应状态码 - 成功 */
    constexpr int32 HTTP_STATUS_OK = 200;

    /** HTTP 响应状态码 - 错误请求 */
    constexpr int32 HTTP_STATUS_BAD_REQUEST = 400;

    /** HTTP 响应状态码 - 内部错误 */
    constexpr int32 HTTP_STATUS_INTERNAL_ERROR = 500;

    // ============================================================================
    // 性能和限制常量
    // ============================================================================

    /** 场景信息返回的最大Actor数量 - 避免响应过大 */
    constexpr int32 MAX_ACTORS_IN_SCENE_INFO = 1000;

    /** 查询返回的最大结果数 */
    constexpr int32 MAX_QUERY_RESULTS = 100;

    /** 命令执行的最大超时时间 (秒) */
    constexpr float MAX_COMMAND_EXECUTION_TIME = 10.0f;

    /** 批处理操作的最大数量 */
    constexpr int32 MAX_BATCH_OPERATIONS = 50;

    // ============================================================================
    // 日志和调试常量
    // ============================================================================

    /** 是否启用详细日志 - 默认关闭以提高性能 */
    constexpr bool DEFAULT_VERBOSE_LOGGING = false;

    /** 日志中显示的最大消息长度 (字符) */
    constexpr int32 MAX_LOG_MESSAGE_LENGTH = 500;

    /** 是否记录完整的JSON消息 - 调试用 */
    constexpr bool LOG_FULL_JSON_MESSAGES = false;

    // ============================================================================
    // 安全常量
    // ============================================================================

    /** 是否启用IP白名单 - 默认关闭,仅本地访问 */
    constexpr bool ENABLE_IP_WHITELIST = false;

    /** 是否只允许本地连接 (localhost) */
    constexpr bool LOCALHOST_ONLY = false;

    /** 最大命令执行深度 - 防止递归调用 */
    constexpr int32 MAX_COMMAND_DEPTH = 5;

    // ============================================================================
    // 路径常量 - 运行时初始化
    // ============================================================================

    /** 项目根目录路径 */
    extern FString ProjectRootPath;

    /** 插件根目录路径 */
    extern FString PluginRootPath;

    /** 插件Content目录路径 */
    extern FString PluginContentPath;

    /** 插件日志目录路径 */
    extern FString PluginLogsPath;

    // ============================================================================
    // 命令名称常量
    // ============================================================================

    /** 获取场景信息命令 */
    static const FString CMD_GET_SCENE_INFO = TEXT("get_scene_info");

    /** 创建对象命令 */
    static const FString CMD_CREATE_OBJECT = TEXT("create_object");

    /** 修改对象命令 */
    static const FString CMD_MODIFY_OBJECT = TEXT("modify_object");

    /** 删除对象命令 */
    static const FString CMD_DELETE_OBJECT = TEXT("delete_object");

    // ============================================================================
    // 函数声明
    // ============================================================================

    /**
     * 初始化路径常量
     * 必须在模块启动时调用以初始化所有路径相关的常量
     */
    void InitializePathConstants();

    /**
     * 验证端口号是否有效
     * @param Port 要验证的端口号
     * @return 如果端口号有效返回true,否则返回false
     */
    bool IsValidPort(int32 Port);

    /**
     * 验证超时时间是否有效
     * @param TimeoutSeconds 要验证的超时时间(秒)
     * @return 如果超时时间有效返回true,否则返回false
     */
    bool IsValidTimeout(float TimeoutSeconds);

    /**
     * 获取安全的日志消息(截断过长的消息)
     * @param Message 原始消息
     * @param MaxLength 最大长度,默认使用 MAX_LOG_MESSAGE_LENGTH
     * @return 截断后的安全消息
     */
    FString GetSafeLogMessage(const FString &Message, int32 MaxLength = MAX_LOG_MESSAGE_LENGTH);
}
