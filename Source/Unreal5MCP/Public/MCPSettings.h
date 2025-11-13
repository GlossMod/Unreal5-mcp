#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MCPConstants.h"
#include "MCPSettings.generated.h"

/**
 * MCP 插件设置
 */
UCLASS(config = Editor, defaultconfig)
class UNREAL5MCP_API UMCPSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** MCP 服务器端口 */
    UPROPERTY(config, EditAnywhere, Category = "MCP Server", meta = (ClampMin = "1024", ClampMax = "65535"))
    int32 Port = MCPConstants::DEFAULT_PORT;

    /** 启用详细日志 */
    UPROPERTY(config, EditAnywhere, Category = "MCP Server")
    bool bEnableVerboseLogging = MCPConstants::DEFAULT_VERBOSE_LOGGING;

    /** 客户端超时时间（秒） */
    UPROPERTY(config, EditAnywhere, Category = "MCP Server", meta = (ClampMin = "5.0", ClampMax = "300.0"))
    float ClientTimeoutSeconds = MCPConstants::DEFAULT_CLIENT_TIMEOUT_SECONDS;
};
