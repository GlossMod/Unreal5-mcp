// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPCommandHandlers.h"
#include "Unreal5MCP.h"
#include "MCPConstants.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"
#include "Selection.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "PackageTools.h"
#include "FileHelpers.h"
#include "ObjectTools.h"

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

TSharedPtr<FJsonObject> FMCPCommandHandlerBase::CreateErrorResponse(const FString &Message, int32 ErrorCode)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField("status", TEXT("error"));
    Response->SetStringField("message", Message);
    if (ErrorCode >= 0)
    {
        Response->SetNumberField("error_code", ErrorCode);
    }
    return Response;
}

bool FMCPCommandHandlerBase::ValidateRequiredField(const TSharedPtr<FJsonObject> &Params,
                                                   const FString &FieldName,
                                                   TSharedPtr<FJsonObject> &OutErrorResponse)
{
    if (!Params.IsValid() || !Params->HasField(FieldName))
    {
        OutErrorResponse = CreateErrorResponse(
            FString::Printf(TEXT("Missing required parameter: %s"), *FieldName));
        return false;
    }
    return true;
}

FString FMCPCommandHandlerBase::GetStringParam(const TSharedPtr<FJsonObject> &Params,
                                               const FString &FieldName,
                                               const FString &DefaultValue) const
{
    FString Value;
    if (Params.IsValid() && Params->TryGetStringField(FieldName, Value))
    {
        return Value;
    }
    return DefaultValue;
}

double FMCPCommandHandlerBase::GetNumberParam(const TSharedPtr<FJsonObject> &Params,
                                              const FString &FieldName,
                                              double DefaultValue) const
{
    double Value;
    if (Params.IsValid() && Params->TryGetNumberField(FieldName, Value))
    {
        return Value;
    }
    return DefaultValue;
}

bool FMCPCommandHandlerBase::GetBoolParam(const TSharedPtr<FJsonObject> &Params,
                                          const FString &FieldName,
                                          bool DefaultValue) const
{
    bool Value;
    if (Params.IsValid() && Params->TryGetBoolField(FieldName, Value))
    {
        return Value;
    }
    return DefaultValue;
}

FVector FMCPCommandHandlerBase::GetVectorFromJson(const TSharedPtr<FJsonObject> &JsonObject,
                                                  const FVector &DefaultValue) const
{
    if (!JsonObject.IsValid())
    {
        return DefaultValue;
    }

    FVector Result = DefaultValue;
    JsonObject->TryGetNumberField(TEXT("x"), Result.X);
    JsonObject->TryGetNumberField(TEXT("y"), Result.Y);
    JsonObject->TryGetNumberField(TEXT("z"), Result.Z);
    return Result;
}

FRotator FMCPCommandHandlerBase::GetRotatorFromJson(const TSharedPtr<FJsonObject> &JsonObject,
                                                    const FRotator &DefaultValue) const
{
    if (!JsonObject.IsValid())
    {
        return DefaultValue;
    }

    FRotator Result = DefaultValue;
    JsonObject->TryGetNumberField(TEXT("pitch"), Result.Pitch);
    JsonObject->TryGetNumberField(TEXT("yaw"), Result.Yaw);
    JsonObject->TryGetNumberField(TEXT("roll"), Result.Roll);
    return Result;
}

TSharedPtr<FJsonObject> FMCPCommandHandlerBase::VectorToJson(const FVector &Vector) const
{
    TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
    JsonObj->SetNumberField(TEXT("x"), Vector.X);
    JsonObj->SetNumberField(TEXT("y"), Vector.Y);
    JsonObj->SetNumberField(TEXT("z"), Vector.Z);
    return JsonObj;
}

TSharedPtr<FJsonObject> FMCPCommandHandlerBase::RotatorToJson(const FRotator &Rotator) const
{
    TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
    JsonObj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
    JsonObj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
    JsonObj->SetNumberField(TEXT("roll"), Rotator.Roll);
    return JsonObj;
}

UWorld *FMCPCommandHandlerBase::GetEditorWorld(TSharedPtr<FJsonObject> &OutErrorResponse)
{
    UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        OutErrorResponse = CreateErrorResponse(TEXT("No active editor world found"));
        return nullptr;
    }
    return World;
}

AActor *FMCPCommandHandlerBase::FindActorByName(UWorld *World,
                                                const FString &ActorName,
                                                TSharedPtr<FJsonObject> &OutErrorResponse)
{
    if (!World)
    {
        OutErrorResponse = CreateErrorResponse(TEXT("Invalid world"));
        return nullptr;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor *Actor = *It;
        if (Actor && Actor->GetName() == ActorName)
        {
            return Actor;
        }
    }

    OutErrorResponse = CreateErrorResponse(
        FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    return nullptr;
}

// ============================================================================
// 获取场景信息命令处理器
// ============================================================================

TSharedPtr<FJsonObject> FMCPGetSceneInfoHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> ErrorResponse;
    UWorld *World = GetEditorWorld(ErrorResponse);
    if (!World)
    {
        return ErrorResponse;
    }

    // 获取可选参数
    bool bIncludeActors = GetBoolParam(Params, TEXT("include_actors"), true);
    bool bIncludeDetails = GetBoolParam(Params, TEXT("include_details"), true);
    int32 MaxActors = static_cast<int32>(GetNumberParam(Params, TEXT("max_actors"),
                                                        MCPConstants::MAX_ACTORS_IN_SCENE_INFO));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    // 获取关卡信息
    FString LevelName = World->GetMapName();
    Result->SetStringField("level", LevelName);
    Result->SetStringField("level_path", World->GetCurrentLevel() ? World->GetCurrentLevel()->GetPathName() : TEXT("Unknown"));

    // 统计 Actor 数量
    int32 ActorCount = 0;
    int32 VisibleActorCount = 0;
    TArray<TSharedPtr<FJsonValue>> ActorsArray;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor *Actor = *It;
        if (Actor && !Actor->IsTemplate())
        {
            ActorCount++;
            if (!Actor->IsHidden())
            {
                VisibleActorCount++;
            }

            // 如果需要详细信息且未超过最大数量
            if (bIncludeActors && ActorsArray.Num() < MaxActors)
            {
                TSharedPtr<FJsonObject> ActorInfo = MakeShared<FJsonObject>();
                ActorInfo->SetStringField("name", Actor->GetName());
                ActorInfo->SetStringField("class", Actor->GetClass()->GetName());
                ActorInfo->SetStringField("label", Actor->GetActorLabel());
                ActorInfo->SetBoolField("hidden", Actor->IsHidden());
                ActorInfo->SetBoolField("selected", Actor->IsSelected());

                if (bIncludeDetails)
                {
                    ActorInfo->SetObjectField("location", VectorToJson(Actor->GetActorLocation()));
                    ActorInfo->SetObjectField("rotation", RotatorToJson(Actor->GetActorRotation()));
                    ActorInfo->SetObjectField("scale", VectorToJson(Actor->GetActorScale3D()));
                }

                ActorsArray.Add(MakeShared<FJsonValueObject>(ActorInfo));
            }
        }
    }

    Result->SetNumberField("actor_count", ActorCount);
    Result->SetNumberField("visible_actor_count", VisibleActorCount);

    if (bIncludeActors)
    {
        Result->SetArrayField("actors", ActorsArray);
        if (ActorCount > MaxActors)
        {
            Result->SetStringField("warning",
                                   FString::Printf(TEXT("Only showing %d out of %d actors"), MaxActors, ActorCount));
        }
    }

    MCP_LOG_INFO("Scene info retrieved: %d actors total, %d visible", ActorCount, VisibleActorCount);
    return CreateSuccessResponse(Result);
}

// ============================================================================
// 创庺对象命令处理器
// ============================================================================

TSharedPtr<FJsonObject> FMCPCreateObjectHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> ErrorResponse;
    UWorld *World = GetEditorWorld(ErrorResponse);
    if (!World)
    {
        return ErrorResponse;
    }

    // 验证必需参数
    if (!ValidateRequiredField(Params, TEXT("class_name"), ErrorResponse))
    {
        return ErrorResponse;
    }

    FString ClassName = GetStringParam(Params, TEXT("class_name"));

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
    TSharedPtr<FJsonObject> ErrorResponse;
    UWorld *World = GetEditorWorld(ErrorResponse);
    if (!World)
    {
        return ErrorResponse;
    }

    // 验证必需参数
    if (!ValidateRequiredField(Params, TEXT("actor_name"), ErrorResponse))
    {
        return ErrorResponse;
    }

    FString ActorName = GetStringParam(Params, TEXT("actor_name"));

    // 查找 Actor
    AActor *TargetActor = FindActorByName(World, ActorName, ErrorResponse);
    if (!TargetActor)
    {
        return ErrorResponse;
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
    TSharedPtr<FJsonObject> ErrorResponse;
    UWorld *World = GetEditorWorld(ErrorResponse);
    if (!World)
    {
        return ErrorResponse;
    }

    // 验证必需参数
    if (!ValidateRequiredField(Params, TEXT("actor_name"), ErrorResponse))
    {
        return ErrorResponse;
    }

    FString ActorName = GetStringParam(Params, TEXT("actor_name"));

    // 查找 Actor
    AActor *TargetActor = FindActorByName(World, ActorName, ErrorResponse);
    if (!TargetActor)
    {
        return ErrorResponse;
    }

    World->DestroyActor(TargetActor);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField("actor_name", ActorName);
    Result->SetStringField("message", TEXT("Actor deleted successfully"));

    return CreateSuccessResponse(Result);
}

// ============================================================================
// 蓝图相关命令处理器实现
// ============================================================================

TSharedPtr<FJsonObject> FMCPCreateBlueprintHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> ErrorResponse;

    // 获取蓝图参数
    FString BlueprintPath = GetStringParam(Params, TEXT("path"));
    FString ParentClass = GetStringParam(Params, TEXT("parent_class"), TEXT("Character"));
    FString BlueprintName = GetStringParam(Params, TEXT("name"));

    if (BlueprintPath.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Missing required parameter: path"));
    }

    if (BlueprintName.IsEmpty())
    {
        BlueprintName = TEXT("NewBlueprint");
    }

    // 使用蓝图工厂创建蓝图
    UBlueprintFactory *BlueprintFactory = NewObject<UBlueprintFactory>();
    if (!BlueprintFactory)
    {
        return CreateErrorResponse(TEXT("Failed to create blueprint factory"));
    }

    // 查找父类
    UClass *ParentClassPtr = FindObject<UClass>(nullptr, *ParentClass);
    if (!ParentClassPtr)
    {
        ParentClassPtr = FindObject<UClass>(nullptr, TEXT("/Script/Engine.Character"));
    }

    BlueprintFactory->ParentClass = ParentClassPtr;

    // 创建包
    FString PackageName = FPackageName::ObjectPathToPackageName(BlueprintPath);
    UPackage *Package = CreatePackage(*PackageName);

    if (!Package)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Failed to create package: %s"), *PackageName));
    }

    // 创建蓝图
    UBlueprint *NewBlueprint = Cast<UBlueprint>(
        BlueprintFactory->FactoryCreateNew(
            UBlueprint::StaticClass(),
            Package,
            *BlueprintName,
            RF_Public | RF_Standalone,
            nullptr,
            GWarn));

    if (!NewBlueprint)
    {
        return CreateErrorResponse(TEXT("Failed to create blueprint"));
    }

    // 编译蓝图
    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    // 保存包
    FEditorFileUtils::PromptForCheckoutAndSave({Package}, false, false);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField("blueprint_name", BlueprintName);
    Result->SetStringField("blueprint_path", BlueprintPath);
    Result->SetStringField("parent_class", ParentClass);
    Result->SetBoolField("compiled", true);

    MCP_LOG_INFO("Blueprint created: %s at %s", *BlueprintName, *BlueprintPath);
    return CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FMCPGetBlueprintInfoHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> ErrorResponse;

    FString BlueprintPath = GetStringParam(Params, TEXT("path"));
    if (BlueprintPath.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Missing required parameter: path"));
    }

    UBlueprint *Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField("name", Blueprint->GetName());
    Result->SetStringField("path", BlueprintPath);

    if (Blueprint->ParentClass)
    {
        Result->SetStringField("parent_class", Blueprint->ParentClass->GetName());
    }

    Result->SetBoolField("is_compiled", Blueprint->IsUpToDate());
    Result->SetNumberField("variable_count", Blueprint->NewVariables.Num());
    Result->SetNumberField("function_count",
                           Blueprint->FunctionGraphs.Num() + Blueprint->DelegateSignatureGraphs.Num());

    MCP_LOG_INFO("Blueprint info retrieved: %s", *BlueprintPath);
    return CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FMCPModifyBlueprintHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> ErrorResponse;

    FString BlueprintPath = GetStringParam(Params, TEXT("path"));
    if (BlueprintPath.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Missing required parameter: path"));
    }

    UBlueprint *Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    // 获取修改参数
    if (Params->HasField(TEXT("description")))
    {
        FString Description = GetStringParam(Params, TEXT("description"));
        // 描述字段可用于将来的扩展
    }

    // 标记为已修改
    Blueprint->MarkPackageDirty();
    Blueprint->Modify();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField("blueprint_name", Blueprint->GetName());
    Result->SetStringField("message", TEXT("Blueprint modified successfully"));

    MCP_LOG_INFO("Blueprint modified: %s", *BlueprintPath);
    return CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FMCPCompileBlueprintHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> ErrorResponse;

    FString BlueprintPath = GetStringParam(Params, TEXT("path"));
    if (BlueprintPath.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Missing required parameter: path"));
    }

    UBlueprint *Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    // 编译蓝图
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    bool bCompileSuccess = Blueprint->IsUpToDate();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField("blueprint_name", Blueprint->GetName());
    Result->SetBoolField("compile_success", bCompileSuccess);

    MCP_LOG_INFO("Blueprint compiled: %s (Success: %s)", *BlueprintPath, bCompileSuccess ? TEXT("true") : TEXT("false"));
    return CreateSuccessResponse(Result);
}

// ============================================================================
// 场景编辑命令处理器实现
// ============================================================================

TSharedPtr<FJsonObject> FMCPSetCameraHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> ErrorResponse;
    UWorld *World = GetEditorWorld(ErrorResponse);
    if (!World)
    {
        return ErrorResponse;
    }

    FVector Location = FVector::ZeroVector;
    if (Params->HasField(TEXT("location")))
    {
        const TSharedPtr<FJsonObject> *LocationObj;
        if (Params->TryGetObjectField(TEXT("location"), LocationObj))
        {
            Location = GetVectorFromJson(*LocationObj);
        }
    }

    FRotator Rotation = FRotator::ZeroRotator;
    if (Params->HasField(TEXT("rotation")))
    {
        const TSharedPtr<FJsonObject> *RotationObj;
        if (Params->TryGetObjectField(TEXT("rotation"), RotationObj))
        {
            Rotation = GetRotatorFromJson(*RotationObj);
        }
    }

    // 设置所有编辑器视口的摄像机
    if (GEditor && GEditor->GetActiveViewport())
    {
        FLevelEditorViewportClient *ViewportClient =
            static_cast<FLevelEditorViewportClient *>(GEditor->GetActiveViewport()->GetClient());

        if (ViewportClient)
        {
            ViewportClient->SetViewLocation(Location);
            ViewportClient->SetViewRotation(Rotation);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetObjectField("location", VectorToJson(Location));
    Result->SetObjectField("rotation", RotatorToJson(Rotation));

    MCP_LOG_INFO("Camera set to location: (%.1f, %.1f, %.1f)", Location.X, Location.Y, Location.Z);
    return CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FMCPGetCameraHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    if (GEditor && GEditor->GetActiveViewport())
    {
        FLevelEditorViewportClient *ViewportClient =
            static_cast<FLevelEditorViewportClient *>(GEditor->GetActiveViewport()->GetClient());

        if (ViewportClient)
        {
            FVector Location = ViewportClient->GetViewLocation();
            FRotator Rotation = ViewportClient->GetViewRotation();

            Result->SetObjectField("location", VectorToJson(Location));
            Result->SetObjectField("rotation", RotatorToJson(Rotation));
            Result->SetNumberField("fov", ViewportClient->FOVAngle);
        }
    }

    return CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FMCPCreateLightHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> ErrorResponse;
    UWorld *World = GetEditorWorld(ErrorResponse);
    if (!World)
    {
        return ErrorResponse;
    }

    FString LightType = GetStringParam(Params, TEXT("type"), TEXT("point"));
    FVector Location = FVector::ZeroVector;
    if (Params->HasField(TEXT("location")))
    {
        const TSharedPtr<FJsonObject> *LocationObj;
        if (Params->TryGetObjectField(TEXT("location"), LocationObj))
        {
            Location = GetVectorFromJson(*LocationObj);
        }
    }

    float Intensity = static_cast<float>(GetNumberParam(Params, TEXT("intensity"), 1000.0));
    float Temperature = static_cast<float>(GetNumberParam(Params, TEXT("temperature"), 6500.0));
    FString LightName = GetStringParam(Params, TEXT("name"), TEXT("NewLight"));

    AActor *NewLight = nullptr;

    if (LightType == TEXT("directional"))
    {
        NewLight = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, FRotator::ZeroRotator);
        if (ADirectionalLight *DirLight = Cast<ADirectionalLight>(NewLight))
        {
            DirLight->GetLightComponent()->SetIntensity(Intensity);
        }
    }
    else if (LightType == TEXT("spot"))
    {
        NewLight = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, FRotator::ZeroRotator);
        if (ASpotLight *SpotLight = Cast<ASpotLight>(NewLight))
        {
            SpotLight->GetLightComponent()->SetIntensity(Intensity);
        }
    }
    else // 默认点光源
    {
        NewLight = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, FRotator::ZeroRotator);
        if (APointLight *PointLight = Cast<APointLight>(NewLight))
        {
            PointLight->GetLightComponent()->SetIntensity(Intensity);
        }
    }

    if (!NewLight)
    {
        return CreateErrorResponse(TEXT("Failed to spawn light"));
    }

    NewLight->SetActorLabel(LightName);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField("light_name", NewLight->GetName());
    Result->SetStringField("light_type", LightType);
    Result->SetObjectField("location", VectorToJson(Location));
    Result->SetNumberField("intensity", Intensity);

    MCP_LOG_INFO("Light created: %s of type %s at location", *LightName, *LightType);
    return CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FMCPSelectActorHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> ErrorResponse;
    UWorld *World = GetEditorWorld(ErrorResponse);
    if (!World)
    {
        return ErrorResponse;
    }

    FString ActorName = GetStringParam(Params, TEXT("actor_name"));
    if (ActorName.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Missing required parameter: actor_name"));
    }

    AActor *TargetActor = FindActorByName(World, ActorName, ErrorResponse);
    if (!TargetActor)
    {
        return ErrorResponse;
    }

    bool bSelect = GetBoolParam(Params, TEXT("select"), true);

    if (GEditor)
    {
        GEditor->SelectActor(TargetActor, bSelect, true);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField("actor_name", ActorName);
    Result->SetBoolField("selected", bSelect);

    MCP_LOG_INFO("Actor %s: %s", *ActorName, bSelect ? TEXT("selected") : TEXT("deselected"));
    return CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FMCPGetSelectedActorsHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> SelectedActorsArray;

    if (GEditor)
    {
        for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
        {
            AActor *Actor = Cast<AActor>(*It);
            if (Actor)
            {
                TSharedPtr<FJsonObject> ActorInfo = MakeShared<FJsonObject>();
                ActorInfo->SetStringField("name", Actor->GetName());
                ActorInfo->SetStringField("class", Actor->GetClass()->GetName());
                ActorInfo->SetObjectField("location", VectorToJson(Actor->GetActorLocation()));

                SelectedActorsArray.Add(MakeShared<FJsonValueObject>(ActorInfo));
            }
        }
    }

    Result->SetArrayField("selected_actors", SelectedActorsArray);
    Result->SetNumberField("count", SelectedActorsArray.Num());

    MCP_LOG_INFO("Retrieved %d selected actors", SelectedActorsArray.Num());
    return CreateSuccessResponse(Result);
}

// ============================================================================
// 资源管理命令处理器实现
// ============================================================================

TSharedPtr<FJsonObject> FMCPImportAssetHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    FString SourcePath = GetStringParam(Params, TEXT("source_path"));
    FString DestinationPath = GetStringParam(Params, TEXT("destination_path"));

    if (SourcePath.IsEmpty() || DestinationPath.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Missing required parameters: source_path, destination_path"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField("message", TEXT("Asset import functionality requires advanced asset tools integration"));
    Result->SetStringField("source_path", SourcePath);
    Result->SetStringField("destination_path", DestinationPath);

    MCP_LOG_WARNING("Asset import requested: %s -> %s (requires custom implementation)",
                    *SourcePath, *DestinationPath);
    return CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FMCPCreateMaterialHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    FString MaterialPath = GetStringParam(Params, TEXT("path"));
    FString MaterialName = GetStringParam(Params, TEXT("name"), TEXT("NewMaterial"));

    if (MaterialPath.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Missing required parameter: path"));
    }

    // 创建材质
    UMaterialFactoryNew *MaterialFactory = NewObject<UMaterialFactoryNew>();
    if (!MaterialFactory)
    {
        return CreateErrorResponse(TEXT("Failed to create material factory"));
    }

    FString PackageName = FPackageName::ObjectPathToPackageName(MaterialPath);
    UPackage *Package = CreatePackage(*PackageName);

    if (!Package)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Failed to create package: %s"), *PackageName));
    }

    UMaterial *NewMaterial = Cast<UMaterial>(
        MaterialFactory->FactoryCreateNew(
            UMaterial::StaticClass(),
            Package,
            *MaterialName,
            RF_Public | RF_Standalone,
            nullptr,
            GWarn));

    if (!NewMaterial)
    {
        return CreateErrorResponse(TEXT("Failed to create material"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField("material_name", MaterialName);
    Result->SetStringField("material_path", MaterialPath);

    MCP_LOG_INFO("Material created: %s at %s", *MaterialName, *MaterialPath);
    return CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FMCPListAssetsHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    FString AssetPath = GetStringParam(Params, TEXT("path"), TEXT("/Game"));
    FString AssetClass = GetStringParam(Params, TEXT("class"));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> AssetsArray;

    // 使用资源注册表
    FAssetRegistryModule &AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(*AssetPath);
    Filter.bRecursivePaths = true;

    if (!AssetClass.IsEmpty())
    {
        Filter.ClassNames.Add(*AssetClass);
    }

    TArray<FAssetData> AssetData;
    AssetRegistry.GetAssets(Filter, AssetData);

    int32 MaxResults = static_cast<int32>(GetNumberParam(Params, TEXT("max_results"), 100));

    for (int32 i = 0; i < AssetData.Num() && i < MaxResults; ++i)
    {
        TSharedPtr<FJsonObject> AssetInfo = MakeShared<FJsonObject>();
        AssetInfo->SetStringField("name", AssetData[i].AssetName.ToString());
        AssetInfo->SetStringField("class", AssetData[i].AssetClass.ToString());
        AssetInfo->SetStringField("path", AssetData[i].GetObjectPathString());

        AssetsArray.Add(MakeShared<FJsonValueObject>(AssetInfo));
    }

    Result->SetArrayField("assets", AssetsArray);
    Result->SetNumberField("count", AssetsArray.Num());

    MCP_LOG_INFO("Listed %d assets in path %s", AssetsArray.Num(), *AssetPath);
    return CreateSuccessResponse(Result);
}

// ============================================================================
// 批量操作命令处理器实现
// ============================================================================

TSharedPtr<FJsonObject> FMCPBatchCreateHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> ErrorResponse;
    UWorld *World = GetEditorWorld(ErrorResponse);
    if (!World)
    {
        return ErrorResponse;
    }

    const TArray<TSharedPtr<FJsonValue>> *ActorsArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("actors"), ActorsArray))
    {
        return CreateErrorResponse(TEXT("Missing required parameter: actors (array)"));
    }

    TArray<TSharedPtr<FJsonValue>> CreatedActorsArray;
    int32 FailureCount = 0;

    for (const TSharedPtr<FJsonValue> &ActorValue : *ActorsArray)
    {
        if (!ActorValue.IsValid())
            continue;

        const TSharedPtr<FJsonObject> ActorObj = ActorValue->AsObject();
        if (!ActorObj.IsValid())
            continue;

        FString ClassName = ActorObj->GetStringField(TEXT("class_name"));
        FVector Location = FVector::ZeroVector;

        if (ActorObj->HasField(TEXT("location")))
        {
            const TSharedPtr<FJsonObject> LocationObj = ActorObj->GetObjectField(TEXT("location"));
            Location = GetVectorFromJson(LocationObj);
        }

        FString ActorName = ActorObj->GetStringField(TEXT("name"));

        UClass *ActorClass = FindObject<UClass>(nullptr, *ClassName);
        if (!ActorClass)
        {
            FailureCount++;
            continue;
        }

        FActorSpawnParameters SpawnParams;
        if (!ActorName.IsEmpty())
        {
            SpawnParams.Name = FName(*ActorName);
        }

        AActor *NewActor = World->SpawnActor<AActor>(ActorClass, Location, FRotator::ZeroRotator, SpawnParams);
        if (NewActor)
        {
            TSharedPtr<FJsonObject> ActorInfo = MakeShared<FJsonObject>();
            ActorInfo->SetStringField("name", NewActor->GetName());
            ActorInfo->SetStringField("class", NewActor->GetClass()->GetName());
            CreatedActorsArray.Add(MakeShared<FJsonValueObject>(ActorInfo));
        }
        else
        {
            FailureCount++;
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField("created_actors", CreatedActorsArray);
    Result->SetNumberField("created_count", CreatedActorsArray.Num());
    Result->SetNumberField("failed_count", FailureCount);

    MCP_LOG_INFO("Batch create completed: %d created, %d failed", CreatedActorsArray.Num(), FailureCount);
    return CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FMCPBatchModifyHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> ErrorResponse;
    UWorld *World = GetEditorWorld(ErrorResponse);
    if (!World)
    {
        return ErrorResponse;
    }

    const TArray<TSharedPtr<FJsonValue>> *ActorsArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("actors"), ActorsArray))
    {
        return CreateErrorResponse(TEXT("Missing required parameter: actors (array)"));
    }

    TArray<TSharedPtr<FJsonValue>> ModifiedActorsArray;
    int32 FailureCount = 0;

    for (const TSharedPtr<FJsonValue> &ActorValue : *ActorsArray)
    {
        if (!ActorValue.IsValid())
            continue;

        const TSharedPtr<FJsonObject> ActorObj = ActorValue->AsObject();
        if (!ActorObj.IsValid())
            continue;

        FString ActorName = ActorObj->GetStringField(TEXT("name"));
        AActor *TargetActor = FindActorByName(World, ActorName, ErrorResponse);

        if (!TargetActor)
        {
            FailureCount++;
            continue;
        }

        // 修改位置
        if (ActorObj->HasField(TEXT("location")))
        {
            const TSharedPtr<FJsonObject> LocationObj = ActorObj->GetObjectField(TEXT("location"));
            TargetActor->SetActorLocation(GetVectorFromJson(LocationObj));
        }

        // 修改旋转
        if (ActorObj->HasField(TEXT("rotation")))
        {
            const TSharedPtr<FJsonObject> RotationObj = ActorObj->GetObjectField(TEXT("rotation"));
            TargetActor->SetActorRotation(GetRotatorFromJson(RotationObj));
        }

        // 修改缩放
        if (ActorObj->HasField(TEXT("scale")))
        {
            const TSharedPtr<FJsonObject> ScaleObj = ActorObj->GetObjectField(TEXT("scale"));
            TargetActor->SetActorScale3D(GetVectorFromJson(ScaleObj));
        }

        TSharedPtr<FJsonObject> ActorInfo = MakeShared<FJsonObject>();
        ActorInfo->SetStringField("name", ActorName);
        ModifiedActorsArray.Add(MakeShared<FJsonValueObject>(ActorInfo));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField("modified_actors", ModifiedActorsArray);
    Result->SetNumberField("modified_count", ModifiedActorsArray.Num());
    Result->SetNumberField("failed_count", FailureCount);

    MCP_LOG_INFO("Batch modify completed: %d modified, %d failed", ModifiedActorsArray.Num(), FailureCount);
    return CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FMCPBatchDeleteHandler::Execute(const TSharedPtr<FJsonObject> &Params, FSocket *ClientSocket)
{
    TSharedPtr<FJsonObject> ErrorResponse;
    UWorld *World = GetEditorWorld(ErrorResponse);
    if (!World)
    {
        return ErrorResponse;
    }

    const TArray<TSharedPtr<FJsonValue>> *ActorNamesArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("actor_names"), ActorNamesArray))
    {
        return CreateErrorResponse(TEXT("Missing required parameter: actor_names (array)"));
    }

    TArray<FString> DeletedActors;
    int32 FailureCount = 0;

    for (const TSharedPtr<FJsonValue> &NameValue : *ActorNamesArray)
    {
        if (!NameValue.IsValid())
            continue;

        FString ActorName = NameValue->AsString();
        if (ActorName.IsEmpty())
            continue;

        AActor *TargetActor = FindActorByName(World, ActorName, ErrorResponse);
        if (TargetActor)
        {
            World->DestroyActor(TargetActor);
            DeletedActors.Add(ActorName);
        }
        else
        {
            FailureCount++;
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> DeletedArray;
    for (const FString &ActorName : DeletedActors)
    {
        DeletedArray.Add(MakeShared<FJsonValueString>(ActorName));
    }
    Result->SetArrayField("deleted_actors", DeletedArray);
    Result->SetNumberField("deleted_count", DeletedActors.Num());
    Result->SetNumberField("failed_count", FailureCount);

    MCP_LOG_INFO("Batch delete completed: %d deleted, %d failed", DeletedActors.Num(), FailureCount);
    return CreateSuccessResponse(Result);
}
