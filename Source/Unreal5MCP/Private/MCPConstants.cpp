// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPConstants.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace MCPConstants
{
    // 定义外部变量
    FString ProjectRootPath;
    FString PluginRootPath;
    FString PluginContentPath;
    FString PluginLogsPath;

    void InitializePathConstants()
    {
        // 获取项目根路径
        ProjectRootPath = FPaths::ProjectDir();

        // 获取插件路径
        TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Unreal5MCP"));
        if (Plugin.IsValid())
        {
            PluginRootPath = Plugin->GetBaseDir();
            PluginContentPath = Plugin->GetContentDir();
        }
        else
        {
            // 如果找不到插件,使用项目插件目录
            PluginRootPath = FPaths::Combine(ProjectRootPath, TEXT("Plugins"), TEXT("unreal-mcp"));
            PluginContentPath = FPaths::Combine(PluginRootPath, TEXT("Content"));
        }

        // 设置日志路径
        PluginLogsPath = FPaths::Combine(PluginRootPath, TEXT("Logs"));

        // 确保目录存在
        IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (!PlatformFile.DirectoryExists(*PluginLogsPath))
        {
            PlatformFile.CreateDirectory(*PluginLogsPath);
        }
    }
}
