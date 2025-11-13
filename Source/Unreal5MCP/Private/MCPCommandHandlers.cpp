// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPCommandHandlers.h"
#include "Unreal5MCP.h"
#include "MCPConstants.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

// ============================================================================
// 基类实现
// ============================================================================

TSharedPtr<FJsonObject> FMCPCommandHandlerBase::CreateSuccessResponse(const TSharedPtr<FJsonObject> &Result)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField("status", TEXT("success"));
    if (Result.IsValid())
    {
        Response->SetObjectField("result", Result);
    }
    return Response;
}

TSharedPtr<FJsonObject> FMCPCommandHandlerBase::CreateErrorResponse(const FString &Message)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField("status", TEXT("error"));
    Response->SetStringField("message", Message);
    return Response;
}

// ============================================================================
// 获取场景信息命令处理器
// ============================================================================

TSharedPtr<FJsonObject> FMCPGetSceneInfoHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return CreateErrorResponse(TEXT("No active world found"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    // 获取关卡名称
    FString LevelName = World->GetMapName();
    Result->SetStringField("level", LevelName);

    // 获取 Actor 数量
    int32 ActorCount = 0;
    TArray<TSharedPtr<FJsonValue>> ActorsArray;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor *Actor = *It;
        if (Actor && !Actor->IsTemplate())
        {
            ActorCount++;

            // 如果 Actor 数量不太多，返回详细信息
            if (ActorCount <= MCPConstants::MAX_ACTORS_IN_SCENE_INFO)
            {
                TSharedPtr<FJsonObject> ActorInfo = MakeShared<FJsonObject>();
                ActorInfo->SetStringField("name", Actor->GetName());
                ActorInfo->SetStringField("class", Actor->GetClass()->GetName());

                FVector Location = Actor->GetActorLocation();
                TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
                LocationObj->SetNumberField("x", Location.X);
                LocationObj->SetNumberField("y", Location.Y);
                LocationObj->SetNumberField("z", Location.Z);
                ActorInfo->SetObjectField("location", LocationObj);

                ActorsArray.Add(MakeShared<FJsonValueObject>(ActorInfo));
            }
        }
    }

    Result->SetNumberField("actor_count", ActorCount);
    Result->SetArrayField("actors", ActorsArray);

    return CreateSuccessResponse(Result);
}

// ============================================================================
// 创建对象命令处理器
// ============================================================================

TSharedPtr<FJsonObject> FMCPCreateObjectHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return CreateErrorResponse(TEXT("No active world found"));
    }

    // 获取参数
    FString ClassName;
    if (!Params->TryGetStringField(TEXT("class_name"), ClassName))
    {
        return CreateErrorResponse(TEXT("Missing 'class_name' parameter"));
    }

    // 获取位置
    FVector Location = FVector::ZeroVector;
    if (Params->HasField(TEXT("location")))
    {
        const TSharedPtr<FJsonObject> *LocationObj;
        if (Params->TryGetObjectField(TEXT("location"), LocationObj))
        {
            (*LocationObj)->TryGetNumberField(TEXT("x"), Location.X);
            (*LocationObj)->TryGetNumberField(TEXT("y"), Location.Y);
            (*LocationObj)->TryGetNumberField(TEXT("z"), Location.Z);
        }
    }

    // 获取旋转
    FRotator Rotation = FRotator::ZeroRotator;
    if (Params->HasField(TEXT("rotation")))
    {
        const TSharedPtr<FJsonObject> *RotationObj;
        if (Params->TryGetObjectField(TEXT("rotation"), RotationObj))
        {
            (*RotationObj)->TryGetNumberField(TEXT("pitch"), Rotation.Pitch);
            (*RotationObj)->TryGetNumberField(TEXT("yaw"), Rotation.Yaw);
            (*RotationObj)->TryGetNumberField(TEXT("roll"), Rotation.Roll);
        }
    }

    // 获取缩放
    FVector Scale = FVector::OneVector;
    if (Params->HasField(TEXT("scale")))
    {
        const TSharedPtr<FJsonObject> *ScaleObj;
        if (Params->TryGetObjectField(TEXT("scale"), ScaleObj))
        {
            (*ScaleObj)->TryGetNumberField(TEXT("x"), Scale.X);
            (*ScaleObj)->TryGetNumberField(TEXT("y"), Scale.Y);
            (*ScaleObj)->TryGetNumberField(TEXT("z"), Scale.Z);
        }
    }

    // 获取名称
    FString ActorName;
    Params->TryGetStringField(TEXT("name"), ActorName);

    // 创建 Actor
    FActorSpawnParameters SpawnParams;
    if (!ActorName.IsEmpty())
    {
        SpawnParams.Name = FName(*ActorName);
    }

    // 查找类
    UClass *ActorClass = FindObject<UClass>(nullptr, *ClassName);
    if (!ActorClass)
    {
        // 尝试查找 StaticMeshActor
        if (ClassName.Contains(TEXT("StaticMesh")))
        {
            ActorClass = AStaticMeshActor::StaticClass();
        }
        else
        {
            return CreateErrorResponse(FString::Printf(TEXT("Class not found: %s"), *ClassName));
        }
    }

    AActor *NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
    if (!NewActor)
    {
        return CreateErrorResponse(TEXT("Failed to spawn actor"));
    }

    NewActor->SetActorScale3D(Scale);

    // 如果有资产路径，加载并设置
    FString AssetPath;
    if (Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        if (AStaticMeshActor *MeshActor = Cast<AStaticMeshActor>(NewActor))
        {
            UStaticMesh *Mesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
            if (Mesh)
            {
                MeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
            }
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField("actor_name", NewActor->GetName());
    Result->SetStringField("actor_class", NewActor->GetClass()->GetName());

    return CreateSuccessResponse(Result);
}

// ============================================================================
// 修改对象命令处理器
// ============================================================================

TSharedPtr<FJsonObject> FMCPModifyObjectHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return CreateErrorResponse(TEXT("No active world found"));
    }

    // 获取 Actor 名称
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // 查找 Actor
    AActor *TargetActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor *Actor = *It;
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // 修改位置
    if (Params->HasField(TEXT("location")))
    {
        const TSharedPtr<FJsonObject> *LocationObj;
        if (Params->TryGetObjectField(TEXT("location"), LocationObj))
        {
            FVector Location = TargetActor->GetActorLocation();
            (*LocationObj)->TryGetNumberField(TEXT("x"), Location.X);
            (*LocationObj)->TryGetNumberField(TEXT("y"), Location.Y);
            (*LocationObj)->TryGetNumberField(TEXT("z"), Location.Z);
            TargetActor->SetActorLocation(Location);
        }
    }

    // 修改旋转
    if (Params->HasField(TEXT("rotation")))
    {
        const TSharedPtr<FJsonObject> *RotationObj;
        if (Params->TryGetObjectField(TEXT("rotation"), RotationObj))
        {
            FRotator Rotation = TargetActor->GetActorRotation();
            (*RotationObj)->TryGetNumberField(TEXT("pitch"), Rotation.Pitch);
            (*RotationObj)->TryGetNumberField(TEXT("yaw"), Rotation.Yaw);
            (*RotationObj)->TryGetNumberField(TEXT("roll"), Rotation.Roll);
            TargetActor->SetActorRotation(Rotation);
        }
    }

    // 修改缩放
    if (Params->HasField(TEXT("scale")))
    {
        const TSharedPtr<FJsonObject> *ScaleObj;
        if (Params->TryGetObjectField(TEXT("scale"), ScaleObj))
        {
            FVector Scale = TargetActor->GetActorScale3D();
            (*ScaleObj)->TryGetNumberField(TEXT("x"), Scale.X);
            (*ScaleObj)->TryGetNumberField(TEXT("y"), Scale.Y);
            (*ScaleObj)->TryGetNumberField(TEXT("z"), Scale.Z);
            TargetActor->SetActorScale3D(Scale);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField("actor_name", TargetActor->GetName());
    Result->SetStringField("message", TEXT("Actor modified successfully"));

    return CreateSuccessResponse(Result);
}

// ============================================================================
// 删除对象命令处理器
// ============================================================================

TSharedPtr<FJsonObject> FMCPDeleteObjectHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return CreateErrorResponse(TEXT("No active world found"));
    }

    // 获取 Actor 名称
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // 查找并删除 Actor
    AActor *TargetActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor *Actor = *It;
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    World->DestroyActor(TargetActor);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField("actor_name", ActorName);
    Result->SetStringField("message", TEXT("Actor deleted successfully"));

    return CreateSuccessResponse(Result);
}

// ============================================================================
// 执行 Python 命令处理器 - 已移除,改用直接UE API操作
// ============================================================================
// 注意: execute_python 命令已不再支持
// 所有操作现在通过直接的UE API命令完成(create_object, modify_object等)
// 这样可以避免Python依赖,提高性能和安全性
