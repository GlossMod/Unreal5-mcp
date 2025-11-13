# Unreal5MCP 插件

Unreal5MCP 是一个虚幻引擎 5 插件，实现了 Model Context Protocol (MCP) 服务器，允许 AI 工具（如 Claude）通过编程方式控制和交互虚幻引擎。

## ⚠️ 免责声明

此插件允许 AI 代理直接修改您的虚幻引擎项目。虽然它是一个强大的工具，但也伴随着风险：

- AI 代理可能对项目进行意外更改
- 文件可能被意外删除或修改
- 项目设置可能被更改
- 资产可能被覆盖

**重要安全措施：**

1. 始终使用源代码管理（如 Git 或 Perforce）
2. 定期备份项目
3. 先在单独的项目中测试插件
4. 在提交更改之前仔细审查

使用此插件即表示您认可：
- 您对项目所做的任何更改负全部责任
- 插件作者对 AI 代理造成的任何损害、数据丢失或问题不承担责任
- 您自行承担使用风险

## 功能特性

- TCP 服务器实现，用于虚幻引擎的远程控制
- 基于 JSON 的命令协议，用于 AI 工具集成
- 编辑器 UI 集成，便于访问 MCP 功能
- 全面的场景操作功能
- Python 脚本执行支持

## 系统要求

- 虚幻引擎 5.x
- 为虚幻引擎配置的 C++ 开发环境
- Windows/Mac/Linux

## 安装

1. 将此插件克隆或复制到项目的 `Plugins` 目录：
   ```
   <ProjectRoot>/Plugins/unreal-mcp/
   ```

2. 右键单击 `.uproject` 文件并选择 "Generate Visual Studio project files"

3. 在 IDE 中构建项目

4. 打开项目并在 Edit > Plugins > Unreal5MCP 中启用插件

5. 在虚幻引擎中启用 Python 插件（Edit > Plugins > Scripting > Python Editor Script Plugin）

6. 重启编辑器

## MCP配置

```json
"unreal5-mcp": {
    "type": "http",
    "url": "http://localhost:13377"
}
```


## 许可证

MIT License

## 致谢

本项目参考了 [kvick-games/UnrealMCP](https://github.com/kvick-games/UnrealMCP) 项目。

## 贡献

欢迎贡献！请随时提交 Pull Request。

## 支持

如有问题或功能请求，请在 GitHub 仓库中创建 issue。
