#pragma once

#include "CoreMinimal.h"
#include "MCPTCPServer.h"

/**
 * FMCPCommandHandlerBase - 命令处理器基类
 *
 * 为所有MCP命令处理器提供通用功能,包括:
 * - 统一的响应格式
 * - 参数验证
 * - 错误处理
 * - 日志记录
 */
class FMCPCommandHandlerBase : public IMCPCommandHandler
{
public:
    virtual ~FMCPCommandHandlerBase() {}

protected:
    /**
     * 创建成功响应
     * @param Result 结果数据对象
     * @return JSON响应对象
     */
    TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject> &Result = nullptr);

    /**
     * 创建错误响应
     * @param Message 错误消息
     * @param ErrorCode 错误代码(可选)
     * @return JSON响应对象
     */
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString &Message, int32 ErrorCode = -1);

    /**
     * 验证必需参数是否存在
     * @param Params 参数对象
     * @param FieldName 字段名
     * @param OutErrorResponse 如果验证失败,输出错误响应
     * @return 如果参数存在返回true
     */
    bool ValidateRequiredField(const TSharedPtr<FJsonObject> &Params,
                               const FString &FieldName,
                               TSharedPtr<FJsonObject> &OutErrorResponse);

    /**
     * 从参数中安全获取字符串
     * @param Params 参数对象
     * @param FieldName 字段名
     * @param DefaultValue 默认值
     * @return 字段值或默认值
     */
    FString GetStringParam(const TSharedPtr<FJsonObject> &Params,
                           const FString &FieldName,
                           const FString &DefaultValue = TEXT("")) const;

    /**
     * 从参数中安全获取数字
     * @param Params 参数对象
     * @param FieldName 字段名
     * @param DefaultValue 默认值
     * @return 字段值或默认值
     */
    double GetNumberParam(const TSharedPtr<FJsonObject> &Params,
                          const FString &FieldName,
                          double DefaultValue = 0.0) const;

    /**
     * 从参数中安全获取布尔值
     * @param Params 参数对象
     * @param FieldName 字段名
     * @param DefaultValue 默认值
     * @return 字段值或默认值
     */
    bool GetBoolParam(const TSharedPtr<FJsonObject> &Params,
                      const FString &FieldName,
                      bool DefaultValue = false) const;

    /**
     * 从JSON对象获取向量
     * @param JsonObject JSON对象
     * @param DefaultValue 默认值
     * @return 向量值
     */
    FVector GetVectorFromJson(const TSharedPtr<FJsonObject> &JsonObject,
                              const FVector &DefaultValue = FVector::ZeroVector) const;

    /**
     * 从JSON对象获取旋转
     * @param JsonObject JSON对象
     * @param DefaultValue 默认值
     * @return 旋转值
     */
    FRotator GetRotatorFromJson(const TSharedPtr<FJsonObject> &JsonObject,
                                const FRotator &DefaultValue = FRotator::ZeroRotator) const;

    /**
     * 将向量转换为JSON对象
     * @param Vector 向量
     * @return JSON对象
     */
    TSharedPtr<FJsonObject> VectorToJson(const FVector &Vector) const;

    /**
     * 将旋转转换为JSON对象
     * @param Rotator 旋转
     * @return JSON对象
     */
    TSharedPtr<FJsonObject> RotatorToJson(const FRotator &Rotator) const;

    /**
     * 获取当前编辑器世界
     * @param OutErrorResponse 如果世界无效,输出错误响应
     * @return 世界指针,失败返回nullptr
     */
    UWorld *GetEditorWorld(TSharedPtr<FJsonObject> &OutErrorResponse);

    /**
     * 在世界中查找Actor
     * @param World 世界
     * @param ActorName Actor名称
     * @param OutErrorResponse 如果未找到,输出错误响应
     * @return Actor指针,失败返回nullptr
     */
    AActor *FindActorByName(UWorld *World,
                            const FString &ActorName,
                            TSharedPtr<FJsonObject> &OutErrorResponse);
};

/**
 * 获取场景信息命令处理器
 */
class FMCPGetSceneInfoHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("get_scene_info"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 创建对象命令处理器
 */
class FMCPCreateObjectHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("create_object"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 修改对象命令处理器
 */
class FMCPModifyObjectHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("modify_object"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 删除对象命令处理器
 */
class FMCPDeleteObjectHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("delete_object"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

// ============================================================================
// 蓝图相关命令处理器
// ============================================================================

/**
 * 创建蓝图类命令处理器
 */
class FMCPCreateBlueprintHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("create_blueprint"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 获取蓝图信息命令处理器
 */
class FMCPGetBlueprintInfoHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("get_blueprint_info"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 修改蓝图属性命令处理器
 */
class FMCPModifyBlueprintHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("modify_blueprint"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 编译蓝图命令处理器
 */
class FMCPCompileBlueprintHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("compile_blueprint"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

// ============================================================================
// 场景编辑命令处理器
// ============================================================================

/**
 * 设置摄像机位置命令处理器
 */
class FMCPSetCameraHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("set_camera"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 获取摄像机信息命令处理器
 */
class FMCPGetCameraHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("get_camera"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 创建光源命令处理器
 */
class FMCPCreateLightHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("create_light"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 选择Actor命令处理器
 */
class FMCPSelectActorHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("select_actor"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 获取选中Actor命令处理器
 */
class FMCPGetSelectedActorsHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("get_selected_actors"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

// ============================================================================
// 资源管理命令处理器
// ============================================================================

/**
 * 导入资源命令处理器
 */
class FMCPImportAssetHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("import_asset"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 创建材质命令处理器
 */
class FMCPCreateMaterialHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("create_material"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 列出资源命令处理器
 */
class FMCPListAssetsHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("list_assets"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

// ============================================================================
// 批量操作命令处理器
// ============================================================================

/**
 * 批量创建对象命令处理器
 */
class FMCPBatchCreateHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("batch_create"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 批量修改对象命令处理器
 */
class FMCPBatchModifyHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("batch_modify"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};

/**
 * 批量删除对象命令处理器
 */
class FMCPBatchDeleteHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("batch_delete"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};
