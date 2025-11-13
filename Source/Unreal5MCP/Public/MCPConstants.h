#pragma once

#include "CoreMinimal.h"

/**
 * 常量定义 - MCP 插件全局常量
 */
namespace MCPConstants
{
    // 网络常量
    constexpr int32 DEFAULT_PORT = 13377;
    constexpr int32 DEFAULT_RECEIVE_BUFFER_SIZE = 65536; // 64KB buffer size
    constexpr int32 DEFAULT_SEND_BUFFER_SIZE = DEFAULT_RECEIVE_BUFFER_SIZE;
    constexpr float DEFAULT_CLIENT_TIMEOUT_SECONDS = 30.0f;
    constexpr float DEFAULT_TICK_INTERVAL_SECONDS = 0.1f;

    // Python 常量
    constexpr const TCHAR *PYTHON_TEMP_DIR_NAME = TEXT("PythonTemp");
    constexpr const TCHAR *PYTHON_TEMP_FILE_PREFIX = TEXT("mcp_temp_script_");

    // 日志常量
    constexpr bool DEFAULT_VERBOSE_LOGGING = false;

    // 性能常量
    constexpr int32 MAX_ACTORS_IN_SCENE_INFO = 1000;

    // 路径常量 - 运行时初始化
    extern FString ProjectRootPath;
    extern FString PluginRootPath;
    extern FString PluginContentPath;
    extern FString PluginLogsPath;
    extern FString PythonTempPath;

    /**
     * 初始化路径常量
     * 在模块启动时调用
     */
    void InitializePathConstants();
}
