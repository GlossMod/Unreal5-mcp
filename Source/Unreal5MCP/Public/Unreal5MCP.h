#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMCP, Log, All);

// 日志宏
#define MCP_LOG_INFO(Format, ...) UE_LOG(LogMCP, Log, TEXT(Format), ##__VA_ARGS__)
#define MCP_LOG_WARNING(Format, ...) UE_LOG(LogMCP, Warning, TEXT(Format), ##__VA_ARGS__)
#define MCP_LOG_ERROR(Format, ...) UE_LOG(LogMCP, Error, TEXT(Format), ##__VA_ARGS__)
#define MCP_LOG_VERBOSE(Format, ...) UE_LOG(LogMCP, Verbose, TEXT(Format), ##__VA_ARGS__)

class FMCPTCPServer;

/**
 * Unreal5MCP 模块类
 */
class FUnreal5MCPModule : public IModuleInterface, public TSharedFromThis<FUnreal5MCPModule>
{
public:
    /** IModuleInterface 实现 */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /**
     * 获取 MCP 服务器实例
     * 外部模块可以使用此方法注册自定义处理器
     */
    UNREAL5MCP_API FMCPTCPServer *GetServer() const { return Server.Get(); }

private:
    void ExtendLevelEditorToolbar();
    void AddToolbarButton(class FToolBarBuilder &Builder);
    void ToggleServer();
    void StartServer();
    void StopServer();
    bool IsServerRunning() const;

    // MCP 控制面板函数
    void OpenMCPControlPanel();
    FReply OpenMCPControlPanel_OnClicked();
    void CloseMCPControlPanel();
    void OnMCPControlPanelClosed(const TSharedRef<class SWindow> &Window);
    TSharedRef<class SWidget> CreateMCPControlPanelContent();
    FReply OnStartServerClicked();
    FReply OnStopServerClicked();

    TUniquePtr<FMCPTCPServer> Server;
    TSharedPtr<class SWindow> MCPControlPanelWindow;
};
