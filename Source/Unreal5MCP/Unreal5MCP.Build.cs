// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Unreal5MCP : ModuleRules
{
    public Unreal5MCP(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // 优化设置
        bUseUnity = false; // 禁用Unity构建以加快增量编译

        PublicIncludePaths.AddRange(
            new string[] {
                Path.Combine(ModuleDirectory, "Public")
            }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                Path.Combine(ModuleDirectory, "Private")
            }
        );

        // 核心公共依赖 - 这些模块的API会暴露给外部
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",              // 核心引擎功能
                "CoreUObject",       // UObject系统
                "Engine",            // 引擎核心
                "Json",              // JSON解析
                "JsonUtilities",     // JSON工具
                "Sockets",           // 网络Socket
                "Networking",        // 网络功能
                "HTTP",              // HTTP协议支持
            }
        );

        // 编辑器私有依赖 - 仅在编辑器模式下需要
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Projects",          // 项目管理
                "InputCore",         // 输入系统
                "EditorFramework",   // 编辑器框架
                "UnrealEd",          // 虚幻编辑器
                "ToolMenus",         // 工具栏菜单
                "Slate",             // Slate UI框架
                "SlateCore",         // Slate核心
                "LevelEditor",       // 关卡编辑器
                "DeveloperSettings", // 开发者设置系统
            }
        );

        // 如果需要动态加载的模块
        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                // 目前没有需要动态加载的模块
            }
        );

        // 编译器定义
        PublicDefinitions.AddRange(
            new string[]
            {
                "UNREAL5MCP_ENABLED=1",
            }
        );

        // 仅在编辑器构建时包含
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
