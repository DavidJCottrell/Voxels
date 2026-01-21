// Copyright Your Company. All Rights Reserved.

#include "VoxelBlueprintLibrary.h"
#include "VoxelWorldManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AVoxelWorldManager* UVoxelBlueprintLibrary::GetVoxelWorldManager(const UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;

    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World) return nullptr;

    for (TActorIterator<AVoxelWorldManager> It(World); It; ++It)
    {
        return *It;
    }

    return nullptr;
}

FVoxel UVoxelBlueprintLibrary::GetVoxelAtPosition(const UObject* WorldContextObject, FVector WorldPosition)
{
    AVoxelWorldManager* Manager = GetVoxelWorldManager(WorldContextObject);
    if (Manager)
    {
        return Manager->GetVoxelAtWorldPosition(WorldPosition);
    }
    return FVoxel(EVoxelType::Air);
}

void UVoxelBlueprintLibrary::SetVoxelAtPosition(const UObject* WorldContextObject, FVector WorldPosition, FVoxel Voxel)
{
    AVoxelWorldManager* Manager = GetVoxelWorldManager(WorldContextObject);
    if (Manager)
    {
        Manager->SetVoxelAtWorldPosition(WorldPosition, Voxel);
    }
}

bool UVoxelBlueprintLibrary::IsVoxelTypeSolid(EVoxelType VoxelType)
{
    return VoxelType != EVoxelType::Air && VoxelType != EVoxelType::Water;
}

bool UVoxelBlueprintLibrary::IsVoxelTypeTransparent(EVoxelType VoxelType)
{
    return VoxelType == EVoxelType::Air || VoxelType == EVoxelType::Water || VoxelType == EVoxelType::Ice;
}

FString UVoxelBlueprintLibrary::GetVoxelTypeName(EVoxelType VoxelType)
{
    switch (VoxelType)
    {
    case EVoxelType::Air:       return TEXT("Air");
    case EVoxelType::Stone:     return TEXT("Stone");
    case EVoxelType::Dirt:      return TEXT("Dirt");
    case EVoxelType::Grass:     return TEXT("Grass");
    case EVoxelType::Sand:      return TEXT("Sand");
    case EVoxelType::Water:     return TEXT("Water");
    case EVoxelType::Snow:      return TEXT("Snow");
    case EVoxelType::Bedrock:   return TEXT("Bedrock");
    case EVoxelType::Gravel:    return TEXT("Gravel");
    case EVoxelType::Clay:      return TEXT("Clay");
    case EVoxelType::Ice:       return TEXT("Ice");
    case EVoxelType::Lava:      return TEXT("Lava");
    default:                    return TEXT("Unknown");
    }
}

FChunkCoord UVoxelBlueprintLibrary::WorldPositionToChunkCoord(const UObject* WorldContextObject, FVector WorldPosition)
{
    AVoxelWorldManager* Manager = GetVoxelWorldManager(WorldContextObject);
    if (Manager)
    {
        return Manager->WorldToChunkCoord(WorldPosition);
    }
    return FChunkCoord(0, 0, 0);
}

float UVoxelBlueprintLibrary::GetTerrainHeight(const UObject* WorldContextObject, float WorldX, float WorldY)
{
    AVoxelWorldManager* Manager = GetVoxelWorldManager(WorldContextObject);
    if (Manager)
    {
        return Manager->GetTerrainHeightAtWorldPosition(WorldX, WorldY);
    }
    return 0.0f;
}

bool UVoxelBlueprintLibrary::VoxelRaycast(const UObject* WorldContextObject, FVector Start, FVector End, FVector& HitPosition, FVector& HitNormal, FVoxel& HitVoxel)
{
    AVoxelWorldManager* Manager = GetVoxelWorldManager(WorldContextObject);
    if (Manager)
    {
        return Manager->VoxelRaycast(Start, End, HitPosition, HitNormal, HitVoxel);
    }
    return false;
}

void UVoxelBlueprintLibrary::DestroyVoxelAtPosition(const UObject* WorldContextObject, FVector WorldPosition)
{
    SetVoxelAtPosition(WorldContextObject, WorldPosition, FVoxel(EVoxelType::Air));
}

void UVoxelBlueprintLibrary::PlaceVoxelAtPosition(const UObject* WorldContextObject, FVector WorldPosition, EVoxelType VoxelType)
{
    SetVoxelAtPosition(WorldContextObject, WorldPosition, FVoxel(VoxelType));
}
