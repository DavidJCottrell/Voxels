// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VoxelTypes.h"
#include "VoxelBlueprintLibrary.generated.h"

class AVoxelWorldManager;

/**
 * Blueprint function library for voxel world operations
 */
UCLASS()
class VOXELWORLD_API UVoxelBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Find the voxel world manager in the level */
    UFUNCTION(BlueprintCallable, Category = "Voxel World", meta = (WorldContext = "WorldContextObject"))
    static AVoxelWorldManager* GetVoxelWorldManager(const UObject* WorldContextObject);

    /** Get voxel at world position */
    UFUNCTION(BlueprintCallable, Category = "Voxel World", meta = (WorldContext = "WorldContextObject"))
    static FVoxel GetVoxelAtPosition(const UObject* WorldContextObject, FVector WorldPosition);

    /** Set voxel at world position */
    UFUNCTION(BlueprintCallable, Category = "Voxel World", meta = (WorldContext = "WorldContextObject"))
    static void SetVoxelAtPosition(const UObject* WorldContextObject, FVector WorldPosition, FVoxel Voxel);

    /** Check if a voxel type is solid */
    UFUNCTION(BlueprintPure, Category = "Voxel World")
    static bool IsVoxelTypeSolid(EVoxelType VoxelType);

    /** Check if a voxel type is transparent */
    UFUNCTION(BlueprintPure, Category = "Voxel World")
    static bool IsVoxelTypeTransparent(EVoxelType VoxelType);

    /** Get voxel type display name */
    UFUNCTION(BlueprintPure, Category = "Voxel World")
    static FString GetVoxelTypeName(EVoxelType VoxelType);

    /** Convert world position to chunk coordinate */
    UFUNCTION(BlueprintPure, Category = "Voxel World", meta = (WorldContext = "WorldContextObject"))
    static FChunkCoord WorldPositionToChunkCoord(const UObject* WorldContextObject, FVector WorldPosition);

    /** Get terrain height at XY position */
    UFUNCTION(BlueprintCallable, Category = "Voxel World", meta = (WorldContext = "WorldContextObject"))
    static float GetTerrainHeight(const UObject* WorldContextObject, float WorldX, float WorldY);

    /** Perform voxel raycast */
    UFUNCTION(BlueprintCallable, Category = "Voxel World", meta = (WorldContext = "WorldContextObject"))
    static bool VoxelRaycast(const UObject* WorldContextObject, FVector Start, FVector End, FVector& HitPosition, FVector& HitNormal, FVoxel& HitVoxel);

    /** Destroy voxel at position (set to air) */
    UFUNCTION(BlueprintCallable, Category = "Voxel World", meta = (WorldContext = "WorldContextObject"))
    static void DestroyVoxelAtPosition(const UObject* WorldContextObject, FVector WorldPosition);

    /** Place voxel at position */
    UFUNCTION(BlueprintCallable, Category = "Voxel World", meta = (WorldContext = "WorldContextObject"))
    static void PlaceVoxelAtPosition(const UObject* WorldContextObject, FVector WorldPosition, EVoxelType VoxelType);
};
