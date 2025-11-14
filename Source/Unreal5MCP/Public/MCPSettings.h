#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MCPConstants.h"
#include "MCPSettings.generated.h"

/**
 * UMCPSettings - MCP 插件配置设置
 *
 * 此类包含MCP服务器的所有可配置选项,可在编辑器的项目设置中进行修改
 * 路径: Edit -> Project Settings -> Plugins -> MCP Settings
 */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "MCP Settings"))
class UNREAL5MCP_API UMCPSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UMCPSettings();

    //~ Begin UDeveloperSettings Interface
    virtual FName GetCategoryName() const override;
    virtual FText GetSectionText() const override;
    virtual FText GetSectionDescription() const override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif
    //~ End UDeveloperSettings Interface

    // ============================================================================
    // 服务器网络配置
    // ============================================================================

    /**
     * MCP 服务器监听端口
     * 范围: 1024-65535
     * 默认: 13377
     * 注意: 修改此项需要重启服务器
     */
    UPROPERTY(config, EditAnywhere, Category = "Server|Network",
              meta = (ClampMin = "1024", ClampMax = "65535",
                      DisplayName = "Server Port",
                      ToolTip = "The TCP port the MCP server will listen on. Requires server restart to take effect."))
    int32 Port;

    /**
     * 客户端超时时间（秒）
     * 如果客户端在此时间内没有任何活动,连接将被关闭
     * 范围: 5.0-300.0秒
     * 默认: 30.0秒
     */
    UPROPERTY(config, EditAnywhere, Category = "Server|Network",
              meta = (ClampMin = "5.0", ClampMax = "300.0",
                      DisplayName = "Client Timeout (Seconds)",
                      ToolTip = "Time in seconds before an idle client connection is closed"))
    float ClientTimeoutSeconds;

    /**
     * 最大并发客户端数量
     * 限制同时连接的客户端数量以保护服务器资源
     * 范围: 1-50
     * 默认: 10
     */
    UPROPERTY(config, EditAnywhere, Category = "Server|Network",
              meta = (ClampMin = "1", ClampMax = "50",
                      DisplayName = "Max Concurrent Clients",
                      ToolTip = "Maximum number of clients that can connect simultaneously"))
    int32 MaxConcurrentClients;

    /**
     * 是否只允许本地连接
     * 启用后,只有来自 localhost (127.0.0.1) 的连接才会被接受
     * 默认: false (允许远程连接)
     */
    UPROPERTY(config, EditAnywhere, Category = "Server|Security",
              meta = (DisplayName = "Localhost Only",
                      ToolTip = "Only accept connections from localhost (127.0.0.1)"))
    bool bLocalhostOnly;

    // ============================================================================
    // 日志和调试配置
    // ============================================================================

    /**
     * 启用详细日志
     * 启用后会记录所有网络活动和命令执行的详细信息
     * 警告: 可能影响性能,建议仅在调试时使用
     * 默认: false
     */
    UPROPERTY(config, EditAnywhere, Category = "Server|Logging",
              meta = (DisplayName = "Enable Verbose Logging",
                      ToolTip = "Enable detailed logging of network activity and command execution. May impact performance."))
    bool bEnableVerboseLogging;

    /**
     * 记录完整JSON消息
     * 启用后会在日志中记录完整的JSON请求和响应
     * 警告: 仅用于调试,会产生大量日志
     * 默认: false
     */
    UPROPERTY(config, EditAnywhere, Category = "Server|Logging",
              meta = (DisplayName = "Log Full JSON Messages",
                      ToolTip = "Log complete JSON request and response messages. For debugging only."))
    bool bLogFullJsonMessages;

    // ============================================================================
    // 性能配置
    // ============================================================================

    /**
     * 服务器Tick间隔（秒）
     * 服务器处理连接和数据的频率
     * 较小的值提供更好的响应性但会增加CPU使用率
     * 范围: 0.01-1.0秒
     * 默认: 0.1秒 (10次/秒)
     */
    UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Server|Performance",
              meta = (ClampMin = "0.01", ClampMax = "1.0",
                      DisplayName = "Server Tick Interval (Seconds)",
                      ToolTip = "How often the server processes connections and data. Lower values = better responsiveness but higher CPU usage."))
    float ServerTickInterval;

    /**
     * 场景信息最大Actor数量
     * get_scene_info命令返回的最大Actor数量
     * 限制响应大小以避免性能问题
     * 范围: 100-10000
     * 默认: 1000
     */
    UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Server|Performance",
              meta = (ClampMin = "100", ClampMax = "10000",
                      DisplayName = "Max Actors in Scene Info",
                      ToolTip = "Maximum number of actors to include in scene info responses"))
    int32 MaxActorsInSceneInfo;

    /**
     * 命令执行超时（秒）
     * 单个命令允许的最大执行时间
     * 超时后命令将被取消
     * 范围: 1.0-60.0秒
     * 默认: 10.0秒
     */
    UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Server|Performance",
              meta = (ClampMin = "1.0", ClampMax = "60.0",
                      DisplayName = "Command Execution Timeout (Seconds)",
                      ToolTip = "Maximum time allowed for a single command execution"))
    float CommandExecutionTimeout;

    // ============================================================================
    // 自动启动配置
    // ============================================================================

    /**
     * 编辑器启动时自动启动服务器
     * 启用后,当编辑器打开时会自动启动MCP服务器
     * 默认: false
     */
    UPROPERTY(config, EditAnywhere, Category = "Server|Auto Start",
              meta = (DisplayName = "Auto Start on Editor Launch",
                      ToolTip = "Automatically start the MCP server when the editor launches"))
    bool bAutoStartOnEditorLaunch;

    // ============================================================================
    // 辅助函数
    // ============================================================================

    /**
     * 验证当前配置是否有效
     * @param OutErrorMessage 如果验证失败,包含错误信息
     * @return 配置有效返回true,否则返回false
     */
    bool ValidateSettings(FString &OutErrorMessage) const;

    /**
     * 重置为默认设置
     */
    void ResetToDefaults();

private:
    /** 验证端口号 */
    bool ValidatePort() const;

    /** 验证超时设置 */
    bool ValidateTimeout() const;
};
