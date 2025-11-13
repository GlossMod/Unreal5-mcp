// Copyright Epic Games, Inc. All Rights Reserved.

#include "Unreal5MCP.h"
#include "MCPTCPServer.h"
#include "MCPSettings.h"
#include "MCPConstants.h"
#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "ISettingsModule.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Application/SlateApplication.h"

// 定义日志类别
DEFINE_LOG_CATEGORY(LogMCP);

#define LOCTEXT_NAMESPACE "FUnreal5MCPModule"

void FUnreal5MCPModule::StartupModule()
{
    // 初始化路径常量
    MCPConstants::InitializePathConstants();

    // 初始化自定义日志类别
    MCP_LOG_INFO("Unreal5MCP Plugin is starting up");

    // 注册设置
    if (ISettingsModule *SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->RegisterSettings("Editor", "Plugins", "MCP Settings",
                                         LOCTEXT("MCPSettingsName", "MCP Settings"),
                                         LOCTEXT("MCPSettingsDescription", "Configure MCP plugin settings"),
                                         GetMutableDefault<UMCPSettings>());
    }

    // 延迟注册工具栏扩展，直到编辑器完全初始化
    if (!IsRunningCommandlet())
    {
        FCoreDelegates::OnPostEngineInit.AddRaw(this, &FUnreal5MCPModule::ExtendLevelEditorToolbar);
    }
}

void FUnreal5MCPModule::ShutdownModule()
{
    MCP_LOG_INFO("Unreal5MCP Plugin is shutting down");

    // 停止服务器
    if (Server)
    {
        Server->Stop();
        Server.Reset();
    }

    // 注销设置
    if (ISettingsModule *SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->UnregisterSettings("Editor", "Plugins", "MCP Settings");
    }

    // 清理工具栏
    UToolMenus::UnregisterOwner(this);
}

void FUnreal5MCPModule::ExtendLevelEditorToolbar()
{
    UToolMenus *ToolMenus = UToolMenus::Get();
    if (!ToolMenus)
    {
        MCP_LOG_WARNING("ToolMenus not available");
        return;
    }

    MCP_LOG_INFO("Extending level editor toolbar");

    // 注册工具栏扩展
    UToolMenu *Menu = ToolMenus->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
    if (Menu)
    {
        FToolMenuSection &Section = Menu->FindOrAddSection("PluginTools");

        FToolMenuEntry &Entry = Section.AddEntry(
            FToolMenuEntry::InitToolBarButton(
                "MCPControl",
                FUIAction(
                    FExecuteAction::CreateRaw(this, &FUnreal5MCPModule::ToggleServer),
                    FCanExecuteAction(),
                    FIsActionChecked::CreateRaw(this, &FUnreal5MCPModule::IsServerRunning)),
                LOCTEXT("MCPControl", "MCP Server"),
                LOCTEXT("MCPControlTooltip", "切换 MCP 服务器（模型上下文协议）"),
                FSlateIcon(),
                EUserInterfaceActionType::ToggleButton));
    }
}

void FUnreal5MCPModule::AddToolbarButton(FToolBarBuilder &Builder)
{
    Builder.AddToolBarButton(
        FUIAction(
            FExecuteAction::CreateRaw(this, &FUnreal5MCPModule::ToggleServer),
            FCanExecuteAction(),
            FIsActionChecked::CreateRaw(this, &FUnreal5MCPModule::IsServerRunning)),
        NAME_None,
        LOCTEXT("MCPControl", "MCP Server"),
        LOCTEXT("MCPControlTooltip", "Toggle MCP Server"),
        FSlateIcon(),
        EUserInterfaceActionType::ToggleButton);
}

void FUnreal5MCPModule::OpenMCPControlPanel()
{
    if (MCPControlPanelWindow.IsValid())
    {
        MCPControlPanelWindow->BringToFront();
        return;
    }

    MCPControlPanelWindow = SNew(SWindow)
                                .Title(LOCTEXT("MCPControlPanel", "MCP Control Panel"))
                                .ClientSize(FVector2D(400, 300))
                                .SupportsMaximize(false)
                                .SupportsMinimize(false);

    MCPControlPanelWindow->SetContent(CreateMCPControlPanelContent());
    MCPControlPanelWindow->SetOnWindowClosed(
        FOnWindowClosed::CreateRaw(this, &FUnreal5MCPModule::OnMCPControlPanelClosed));

    FSlateApplication::Get().AddWindow(MCPControlPanelWindow.ToSharedRef());
}

FReply FUnreal5MCPModule::OpenMCPControlPanel_OnClicked()
{
    OpenMCPControlPanel();
    return FReply::Handled();
}

void FUnreal5MCPModule::CloseMCPControlPanel()
{
    if (MCPControlPanelWindow.IsValid())
    {
        MCPControlPanelWindow->RequestDestroyWindow();
        MCPControlPanelWindow.Reset();
    }
}

void FUnreal5MCPModule::OnMCPControlPanelClosed(const TSharedRef<SWindow> &Window)
{
    MCPControlPanelWindow.Reset();
}

TSharedRef<SWidget> FUnreal5MCPModule::CreateMCPControlPanelContent()
{
    return SNew(SBox)
        .Padding(20)
            [SNew(SVerticalBox)

             // 标题
             + SVerticalBox::Slot()
                   .AutoHeight()
                   .Padding(0, 0, 0, 20)
                       [SNew(STextBlock)
                            .Text(LOCTEXT("MCPControlPanelTitle", "MCP Server Control"))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))]

             // 服务器状态
             + SVerticalBox::Slot()
                   .AutoHeight()
                   .Padding(0, 0, 0, 10)
                       [SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(LOCTEXT("ServerStatus", "Server Status: "))] + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text_Lambda([this]() -> FText
                                                                                                                                                                                                                       { return IsServerRunning() ? LOCTEXT("Running", "Running") : LOCTEXT("Stopped", "Stopped"); })
                                                                                                                                                                                              .ColorAndOpacity_Lambda([this]() -> FSlateColor
                                                                                                                                                                                                                      { return IsServerRunning() ? FSlateColor(FLinearColor::Green) : FSlateColor(FLinearColor::Red); })]]

             // 端口信息
             + SVerticalBox::Slot()
                   .AutoHeight()
                   .Padding(0, 0, 0, 20)
                       [SNew(STextBlock)
                            .Text_Lambda([this]() -> FText
                                         {
					const UMCPSettings* Settings = GetDefault<UMCPSettings>();
					return FText::Format(
						LOCTEXT("PortInfo", "Port: {0}"),
						FText::AsNumber(Settings->Port)
					); })]

             // 按钮
             + SVerticalBox::Slot()
                   .AutoHeight()
                       [SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 5, 0)[SNew(SButton).Text(LOCTEXT("StartServer", "Start Server")).OnClicked(this, &FUnreal5MCPModule::OnStartServerClicked).IsEnabled_Lambda([this]()
                                                                                                                                                                                                                                                { return !IsServerRunning(); })] +
                        SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .Padding(5, 0, 0, 0)
                                [SNew(SButton)
                                     .Text(LOCTEXT("StopServer", "Stop Server"))
                                     .OnClicked(this, &FUnreal5MCPModule::OnStopServerClicked)
                                     .IsEnabled_Lambda([this]()
                                                       { return IsServerRunning(); })]]

             // 信息文本
             + SVerticalBox::Slot()
                   .AutoHeight()
                   .Padding(0, 20, 0, 0)
                       [SNew(STextBlock)
                            .Text(LOCTEXT("MCPInfo", "The MCP (Model Context Protocol) server allows AI tools like Claude to control Unreal Engine programmatically."))
                            .AutoWrapText(true)]];
}

FReply FUnreal5MCPModule::OnStartServerClicked()
{
    StartServer();
    return FReply::Handled();
}

FReply FUnreal5MCPModule::OnStopServerClicked()
{
    StopServer();
    return FReply::Handled();
}

void FUnreal5MCPModule::ToggleServer()
{
    MCP_LOG_INFO("ToggleServer called - Server state: %s", (Server && Server->IsRunning()) ? TEXT("Running") : TEXT("Not Running"));

    if (Server && Server->IsRunning())
    {
        MCP_LOG_INFO("Stopping server...");
        StopServer();
    }
    else
    {
        MCP_LOG_INFO("Starting server...");
        StartServer();
    }

    MCP_LOG_INFO("ToggleServer completed - Server state: %s", (Server && Server->IsRunning()) ? TEXT("Running") : TEXT("Not Running"));
}

void FUnreal5MCPModule::StartServer()
{
    // 检查服务器是否已经在运行
    if (Server && Server->IsRunning())
    {
        MCP_LOG_WARNING("Server is already running, ignoring start request");
        return;
    }

    MCP_LOG_INFO("Creating new server instance");
    const UMCPSettings *Settings = GetDefault<UMCPSettings>();

    // 创建配置对象并从设置中设置端口
    FMCPTCPServerConfig Config;
    Config.Port = Settings->Port;
    Config.bEnableVerboseLogging = Settings->bEnableVerboseLogging;
    Config.ClientTimeoutSeconds = Settings->ClientTimeoutSeconds;

    // 使用配置创建服务器
    Server = MakeUnique<FMCPTCPServer>(Config);

    if (Server->Start())
    {
        MCP_LOG_INFO("MCP Server started successfully");

        // 刷新工具栏以更新状态指示器
        if (UToolMenus *ToolMenus = UToolMenus::Get())
        {
            ToolMenus->RefreshAllWidgets();
        }
    }
    else
    {
        MCP_LOG_ERROR("Failed to start MCP Server");
        Server.Reset();
    }
}

void FUnreal5MCPModule::StopServer()
{
    if (Server)
    {
        Server->Stop();
        Server.Reset();
        MCP_LOG_INFO("MCP Server stopped");

        // 刷新工具栏以更新状态指示器
        if (UToolMenus *ToolMenus = UToolMenus::Get())
        {
            ToolMenus->RefreshAllWidgets();
        }
    }
}

bool FUnreal5MCPModule::IsServerRunning() const
{
    return Server && Server->IsRunning();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnreal5MCPModule, Unreal5MCP)
