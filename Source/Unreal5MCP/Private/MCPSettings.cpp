// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPSettings.h"
#include "Unreal5MCP.h"
#include "MCPConstants.h"

#define LOCTEXT_NAMESPACE "MCPSettings"

UMCPSettings::UMCPSettings()
{
    // 初始化默认值
    Port = MCPConstants::DEFAULT_PORT;
    ClientTimeoutSeconds = MCPConstants::DEFAULT_CLIENT_TIMEOUT_SECONDS;
    MaxConcurrentClients = MCPConstants::MAX_CONCURRENT_CLIENTS;
    bLocalhostOnly = MCPConstants::LOCALHOST_ONLY;
    bEnableVerboseLogging = MCPConstants::DEFAULT_VERBOSE_LOGGING;
    bLogFullJsonMessages = MCPConstants::LOG_FULL_JSON_MESSAGES;
    ServerTickInterval = MCPConstants::DEFAULT_TICK_INTERVAL_SECONDS;
    MaxActorsInSceneInfo = MCPConstants::MAX_ACTORS_IN_SCENE_INFO;
    CommandExecutionTimeout = MCPConstants::MAX_COMMAND_EXECUTION_TIME;
    bAutoStartOnEditorLaunch = false;
}

FName UMCPSettings::GetCategoryName() const
{
    return FName(TEXT("Plugins"));
}

FText UMCPSettings::GetSectionText() const
{
    return LOCTEXT("MCPSettingsSectionText", "MCP Settings");
}

FText UMCPSettings::GetSectionDescription() const
{
    return LOCTEXT("MCPSettingsSectionDescription",
                   "Configure the Model Context Protocol (MCP) server settings for Unreal Engine integration");
}

#if WITH_EDITOR
void UMCPSettings::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property == nullptr)
    {
        return;
    }

    const FName PropertyName = PropertyChangedEvent.Property->GetFName();

    // 验证端口设置
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPSettings, Port))
    {
        if (!ValidatePort())
        {
            MCP_LOG_WARNING("Invalid port number %d, resetting to default %d",
                            Port, MCPConstants::DEFAULT_PORT);
            Port = MCPConstants::DEFAULT_PORT;
        }
        else
        {
            MCP_LOG_INFO("Port changed to %d (restart server to apply)", Port);
        }
    }

    // 验证超时设置
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPSettings, ClientTimeoutSeconds))
    {
        if (!ValidateTimeout())
        {
            MCP_LOG_WARNING("Invalid timeout value %.1f, resetting to default %.1f",
                            ClientTimeoutSeconds, MCPConstants::DEFAULT_CLIENT_TIMEOUT_SECONDS);
            ClientTimeoutSeconds = MCPConstants::DEFAULT_CLIENT_TIMEOUT_SECONDS;
        }
    }

    // 验证最大客户端数
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPSettings, MaxConcurrentClients))
    {
        if (MaxConcurrentClients < 1 || MaxConcurrentClients > 50)
        {
            MCP_LOG_WARNING("Invalid max concurrent clients %d, resetting to default %d",
                            MaxConcurrentClients, MCPConstants::MAX_CONCURRENT_CLIENTS);
            MaxConcurrentClients = MCPConstants::MAX_CONCURRENT_CLIENTS;
        }
    }

    // 日志设置更改
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPSettings, bEnableVerboseLogging))
    {
        MCP_LOG_INFO("Verbose logging %s", bEnableVerboseLogging ? TEXT("enabled") : TEXT("disabled"));
    }

    if (PropertyName == GET_MEMBER_NAME_CHECKED(UMCPSettings, bLogFullJsonMessages))
    {
        MCP_LOG_INFO("Full JSON message logging %s", bLogFullJsonMessages ? TEXT("enabled") : TEXT("disabled"));
    }

    // 保存设置
    SaveConfig();
}
#endif

bool UMCPSettings::ValidateSettings(FString &OutErrorMessage) const
{
    // 验证端口
    if (!ValidatePort())
    {
        OutErrorMessage = FString::Printf(TEXT("Invalid port number %d. Must be between %d and %d."),
                                          Port, MCPConstants::MIN_PORT, MCPConstants::MAX_PORT);
        return false;
    }

    // 验证超时
    if (!ValidateTimeout())
    {
        OutErrorMessage = FString::Printf(TEXT("Invalid timeout value %.1f. Must be between %.1f and %.1f seconds."),
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

    // 验证Tick间隔
    if (ServerTickInterval < 0.01f || ServerTickInterval > 1.0f)
    {
        OutErrorMessage = FString::Printf(TEXT("Invalid server tick interval %.2f. Must be between 0.01 and 1.0 seconds."),
                                          ServerTickInterval);
        return false;
    }

    // 验证最大Actor数量
    if (MaxActorsInSceneInfo < 100 || MaxActorsInSceneInfo > 10000)
    {
        OutErrorMessage = FString::Printf(TEXT("Invalid max actors in scene info %d. Must be between 100 and 10000."),
                                          MaxActorsInSceneInfo);
        return false;
    }

    // 验证命令执行超时
    if (CommandExecutionTimeout < 1.0f || CommandExecutionTimeout > 60.0f)
    {
        OutErrorMessage = FString::Printf(TEXT("Invalid command execution timeout %.1f. Must be between 1.0 and 60.0 seconds."),
                                          CommandExecutionTimeout);
        return false;
    }

    OutErrorMessage.Empty();
    return true;
}

void UMCPSettings::ResetToDefaults()
{
    Port = MCPConstants::DEFAULT_PORT;
    ClientTimeoutSeconds = MCPConstants::DEFAULT_CLIENT_TIMEOUT_SECONDS;
    MaxConcurrentClients = MCPConstants::MAX_CONCURRENT_CLIENTS;
    bLocalhostOnly = MCPConstants::LOCALHOST_ONLY;
    bEnableVerboseLogging = MCPConstants::DEFAULT_VERBOSE_LOGGING;
    bLogFullJsonMessages = MCPConstants::LOG_FULL_JSON_MESSAGES;
    ServerTickInterval = MCPConstants::DEFAULT_TICK_INTERVAL_SECONDS;
    MaxActorsInSceneInfo = MCPConstants::MAX_ACTORS_IN_SCENE_INFO;
    CommandExecutionTimeout = MCPConstants::MAX_COMMAND_EXECUTION_TIME;
    bAutoStartOnEditorLaunch = false;

    SaveConfig();
    MCP_LOG_INFO("MCP Settings reset to defaults");
}

bool UMCPSettings::ValidatePort() const
{
    return MCPConstants::IsValidPort(Port);
}

bool UMCPSettings::ValidateTimeout() const
{
    return MCPConstants::IsValidTimeout(ClientTimeoutSeconds);
}

#undef LOCTEXT_NAMESPACE
