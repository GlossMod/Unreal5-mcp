// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPConstants.h"
#include "Unreal5MCP.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

namespace MCPConstants
{
    // ============================================================================
    // 外部变量定义
    // ============================================================================

    FString ProjectRootPath;
    FString PluginRootPath;
    FString PluginContentPath;
    FString PluginLogsPath;

    // ============================================================================
    // 路径初始化
    // ============================================================================

    void InitializePathConstants()
    {
        // 获取项目根路径
        ProjectRootPath = FPaths::ProjectDir();
        MCP_LOG_INFO("Project Root Path: %s", *ProjectRootPath);

        // 获取插件路径
        TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Unreal5MCP"));
        if (Plugin.IsValid())
        {
            PluginRootPath = Plugin->GetBaseDir();
            PluginContentPath = Plugin->GetContentDir();
            MCP_LOG_INFO("Plugin found - Root: %s", *PluginRootPath);
        }
        else
        {
            // 如果找不到插件,使用项目插件目录
            PluginRootPath = FPaths::Combine(ProjectRootPath, TEXT("Plugins"), TEXT("unreal5-mcp"));
            PluginContentPath = FPaths::Combine(PluginRootPath, TEXT("Content"));
            MCP_LOG_WARNING("Plugin not found in plugin manager, using default path: %s", *PluginRootPath);
        }

        // 设置日志路径
        PluginLogsPath = FPaths::Combine(PluginRootPath, TEXT("Logs"));

        // 确保目录存在
        IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (!PlatformFile.DirectoryExists(*PluginLogsPath))
        {
            if (PlatformFile.CreateDirectory(*PluginLogsPath))
            {
                MCP_LOG_INFO("Created logs directory: %s", *PluginLogsPath);
            }
            else
            {
                MCP_LOG_ERROR("Failed to create logs directory: %s", *PluginLogsPath);
            }
        }
    }

    // ============================================================================
    // 验证函数实现
    // ============================================================================

    bool IsValidPort(int32 Port)
    {
        return Port >= MIN_PORT && Port <= MAX_PORT;
    }

    bool IsValidTimeout(float TimeoutSeconds)
    {
        return TimeoutSeconds >= MIN_CLIENT_TIMEOUT_SECONDS &&
               TimeoutSeconds <= MAX_CLIENT_TIMEOUT_SECONDS;
    }

    FString GetSafeLogMessage(const FString &Message, int32 MaxLength)
    {
        if (Message.Len() <= MaxLength)
        {
            return Message;
        }

        return Message.Left(MaxLength) + TEXT("... (truncated)");
    }
}
