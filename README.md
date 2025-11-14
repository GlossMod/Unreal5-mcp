# Unreal5MCP 插件 - 完整文档

Unreal5MCP 是一个虚幻引擎 5 插件,实现了 Model Context Protocol (MCP) 服务器,允许 AI 工具（如 Claude）通过编程方式控制和交互虚幻引擎。

## ⚠️ 免责声明

此插件允许 AI 代理直接修改您的虚幻引擎项目。虽然它是一个强大的工具,但也伴随着风险：

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

### 核心功能
- ✅ TCP 服务器实现,用于虚幻引擎的远程控制
- ✅ 基于 JSON-RPC 的命令协议,用于 AI 工具集成
- ✅ 编辑器 UI 集成,便于访问 MCP 功能
- ✅ 全面的场景操作功能
- ✅ 蓝图创建和管理
- ✅ 灯光、摄像机等场景编辑工具
- ✅ 资源管理和材质创建
- ✅ 批量操作支持

### 支持的平台
- Windows (Win64)
- macOS
- Linux

## 系统要求

- Unreal Engine 5.0 或更高版本（推荐 5.7）
- C++ 开发环境配置完成
- 至少 2GB 磁盘空间

## 安装指南

### 1. 克隆/复制插件文件

将插件放置到项目的 `Plugins` 目录：
```
<ProjectRoot>/Plugins/unreal5-mcp/
```

### 2. 生成 Visual Studio 项目文件

右键单击 `.uproject` 文件，选择 "Generate Visual Studio project files"

### 3. 构建项目

- 在 IDE 中打开解决方案
- 编译项目（Debug 或 Development）

### 4. 启用插件

1. 打开 Unreal Engine 编辑器
2. 进入 Edit → Plugins
3. 搜索 "Unreal5MCP"
4. 启用插件
5. 重启编辑器

### 5. 配置设置

在 Edit → Project Settings → Plugins → MCP Settings 中配置：
- **Server Port**: MCP 服务器端口（默认 13377）
- **Client Timeout**: 客户端超时时间（默认 30 秒）
- **Max Concurrent Clients**: 最大并发客户端数（默认 10）
- **Enable Verbose Logging**: 启用详细日志
- **Auto Start on Editor Launch**: 编辑器启动时自动启动服务器

## 命令参考

### 基础场景操作

#### `get_scene_info` - 获取场景信息
获取当前关卡的信息，包括 Actor 列表、位置、旋转等。

**参数:**
- `include_actors`: 是否包含 Actor 列表（默认 true）
- `include_details`: 是否包含位置、旋转等详细信息（默认 true）
- `max_actors`: 返回的最大 Actor 数量（默认 1000）

#### `create_object` - 创建对象
在场景中创建新对象。

**参数:**
- `class_name`: 要创建的类的完整路径（必需）
- `name`: 对象名称（可选）
- `location`: 位置坐标 {x, y, z}（可选）
- `rotation`: 旋转欧拉角 {pitch, yaw, roll}（可选）
- `scale`: 缩放 {x, y, z}（可选）
- `asset_path`: 资源路径（对于 StaticMeshActor，可选）

#### `modify_object` - 修改对象
修改场景中现有对象的属性。

**参数:**
- `actor_name`: 要修改的 Actor 名称（必需）
- `location`: 新位置（可选）
- `rotation`: 新旋转（可选）
- `scale`: 新缩放（可选）

#### `delete_object` - 删除对象
从场景中删除对象。

**参数:**
- `actor_name`: 要删除的 Actor 名称（必需）

### 蓝图相关操作

#### `create_blueprint` - 创建蓝图
创建新的蓝图类。

**参数:**
- `path`: 蓝图保存路径（必需）
- `name`: 蓝图名称（可选）
- `parent_class`: 父类路径（可选，默认 Character）

#### `get_blueprint_info` - 获取蓝图信息
获取蓝图的详细信息。

**参数:**
- `path`: 蓝图路径（必需）

#### `modify_blueprint` - 修改蓝图
修改蓝图属性。

**参数:**
- `path`: 蓝图路径（必需）
- `description`: 蓝图描述（可选）

#### `compile_blueprint` - 编译蓝图
编译指定的蓝图。

**参数:**
- `path`: 蓝图路径（必需）

### 场景编辑操作

#### `set_camera` - 设置摄像机
设置编辑器视口的摄像机位置和旋转。

**参数:**
- `location`: 摄像机位置 {x, y, z}（可选）
- `rotation`: 摄像机旋转 {pitch, yaw, roll}（可选）

#### `get_camera` - 获取摄像机信息
获取当前编辑器视口摄像机的信息。

#### `create_light` - 创建灯光
在场景中创建灯光。

**参数:**
- `type`: 灯光类型 - "directional"、"point"（默认）、"spot"
- `name`: 灯光名称（可选）
- `location`: 位置 {x, y, z}（可选）
- `intensity`: 强度（默认 1000）
- `temperature`: 色温（默认 6500K）

#### `select_actor` - 选择 Actor
在编辑器中选择或取消选择 Actor。

**参数:**
- `actor_name`: Actor 名称（必需）
- `select`: 是否选择（默认 true）

#### `get_selected_actors` - 获取选中的 Actor
获取当前在编辑器中选中的所有 Actor。

### 资源管理操作

#### `create_material` - 创建材质
创建新材质。

**参数:**
- `path`: 材质保存路径（必需）
- `name`: 材质名称（可选）

#### `list_assets` - 列出资源
列出指定路径下的资源。

**参数:**
- `path`: 搜索路径（默认 /Game）
- `class`: 资源类型过滤（可选）
- `max_results`: 返回的最大结果数（默认 100）

#### `import_asset` - 导入资源
导入外部资源文件。

**参数:**
- `source_path`: 源文件路径（必需）
- `destination_path`: 目标资源路径（必需）

### 批量操作

#### `batch_create` - 批量创建对象
一次性创建多个对象。

**参数:**
- `actors`: 对象数组，每个对象包含 class_name、name、location 等

#### `batch_modify` - 批量修改对象
一次性修改多个对象。

**参数:**
- `actors`: 对象数组，每个对象包含 name、location、rotation、scale 等

#### `batch_delete` - 批量删除对象
一次性删除多个对象。

**参数:**
- `actor_names`: Actor 名称数组

## 使用示例

### Python 示例

```python
import requests
import json

# 服务器配置
SERVER_URL = "http://localhost:13377"

def call_mcp(method_name, arguments):
    """调用 MCP 命令"""
    request_data = {
        "jsonrpc": "2.0",
        "method": "tools/call",
        "params": {
            "name": method_name,
            "arguments": arguments
        },
        "id": 1
    }
    
    response = requests.post(SERVER_URL, json=request_data)
    return response.json()

# 示例：获取场景信息
result = call_mcp("get_scene_info", {
    "include_actors": True,
    "max_actors": 100
})
print(json.dumps(result, indent=2))

# 示例：创建对象
result = call_mcp("create_object", {
    "class_name": "/Script/Engine.StaticMeshActor",
    "name": "MyActor",
    "location": {"x": 0, "y": 0, "z": 0}
})
print(json.dumps(result, indent=2))

# 示例：批量创建
result = call_mcp("batch_create", {
    "actors": [
        {
            "class_name": "/Script/Engine.StaticMeshActor",
            "name": f"Actor_{i}",
            "location": {"x": i * 100, "y": 0, "z": 0}
        }
        for i in range(5)
    ]
})
print(json.dumps(result, indent=2))
```

## 日志位置

MCP 服务器日志保存在：
```
<ProjectRoot>/Plugins/unreal5-mcp/Logs/
```

## 许可证

MIT License

## 致谢

本项目参考了 [kvick-games/UnrealMCP](https://github.com/kvick-games/UnrealMCP) 项目。

## 贡献

欢迎贡献！请随时提交 Pull Request。

## 支持

如有问题或功能请求，请在 GitHub 仓库中创建 issue。

## 常见问题

### Q: 如何重启服务器？
A: 在编辑器工具栏中点击 "MCP Server" 按钮即可切换服务器状态。

### Q: 服务器崩溃了怎么办？
A: 检查 Logs 目录中的日志文件，查看具体错误信息。

### Q: 如何在运行游戏时使用 MCP？
A: 目前 MCP 仅在编辑器模式下运行。如需在运行时使用，需要自定义扩展。

### Q: 支持多个并发客户端吗？
A: 是的，支持多达 10 个并发客户端（可在设置中调整）。

### Q: 如何禁用 MCP 服务器？
A: 在项目设置中禁用 Unreal5MCP 插件，或在启动时不启动服务器。

### Q: 支持的最大命令大小是多少？
A: 单个命令消息的最大大小是 1MB。
