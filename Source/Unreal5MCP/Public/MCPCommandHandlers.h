#pragma once

#include "CoreMinimal.h"
#include "MCPTCPServer.h"

/**
 * 命令处理器基类
 */
class FMCPCommandHandlerBase : public IMCPCommandHandler
{
public:
    virtual ~FMCPCommandHandlerBase() {}

protected:
    /**
     * 创建成功响应
     */
    TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject> &Result);

    /**
     * 创建错误响应
     */
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString &Message);
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

/**
 * 执行 Python 命令处理器
 */
class FMCPExecutePythonHandler : public FMCPCommandHandlerBase
{
public:
    virtual FString GetCommandName() const override { return TEXT("execute_python"); }
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket) override;
};
