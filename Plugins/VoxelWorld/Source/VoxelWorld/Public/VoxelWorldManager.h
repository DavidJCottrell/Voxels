// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelTypes.h"
#include "VoxelWorldManager.generated.h"

class AVoxelChunk;
class UVoxelTerrainGenerator;

/** Async task for chunk generation */
class FChunkGenerationTask : public FNonAbandonableTask
{
public:
    FChunkGenerationTask(AVoxelChunk* InChunk)
        : Chunk(InChunk)
    {
    }

    FORCEINLINE TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FChunkGenerationTask, STATGROUP_ThreadPoolAsyncTasks);
    }

    void DoWork();

private:
    AVoxelChunk* Chunk;
};

/**
 * Main voxel world manager
 * Handles chunk loading, unloading, and world generation
 */
UCLASS()
class VOXELWORLD_API AVoxelWorldManager : public AActor
{
    GENERATED_BODY()

public:
    AVoxelWorldManager();

    virtual void Tick(float DeltaTime) override;

    /** World generation settings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel World")
    FVoxelWorldSettings WorldSettings;

    /** Material to apply to voxel meshes */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel World")
    TObjectPtr<UMaterialInterface> VoxelMaterial;

    /** Initialize and start world generation */
    UFUNCTION(BlueprintCallable, Category = "Voxel World")
    void InitializeWorld();

    /** Set the center position for chunk loading (usually player position) */
    UFUNCTION(BlueprintCallable, Category = "Voxel World")
    void SetLoadCenter(const FVector& WorldPosition);

    /** Get voxel at world position */
    UFUNCTION(BlueprintCallable, Category = "Voxel World")
    FVoxel GetVoxelAtWorldPosition(const FVector& WorldPosition) const;

    /** Set voxel at world position */
    UFUNCTION(BlueprintCallable, Category = "Voxel World")
    void SetVoxelAtWorldPosition(const FVector& WorldPosition, const FVoxel& Voxel);

    /** Get chunk at chunk coordinate */
    UFUNCTION(BlueprintCallable, Category = "Voxel World")
    AVoxelChunk* GetChunk(const FChunkCoord& ChunkCoord) const;

    /** Convert world position to chunk coordinate */
    UFUNCTION(BlueprintCallable, Category = "Voxel World")
    FChunkCoord WorldToChunkCoord(const FVector& WorldPosition) const;

    /** Convert world position to local voxel coordinate within chunk */
    UFUNCTION(BlueprintCallable, Category = "Voxel World")
    void WorldToLocalVoxelCoord(const FVector& WorldPosition, FChunkCoord& OutChunkCoord, int32& OutLocalX, int32& OutLocalY, int32& OutLocalZ) const;

    /** Get terrain height at world XY position */
    UFUNCTION(BlueprintCallable, Category = "Voxel World")
    float GetTerrainHeightAtWorldPosition(float WorldX, float WorldY) const;

    /** Raycast through voxels */
    UFUNCTION(BlueprintCallable, Category = "Voxel World")
    bool VoxelRaycast(const FVector& Start, const FVector& End, FVector& OutHitPosition, FVector& OutHitNormal, FVoxel& OutHitVoxel) const;

    /** Get statistics about loaded chunks */
    UFUNCTION(BlueprintCallable, Category = "Voxel World")
    void GetChunkStats(int32& OutLoadedChunks, int32& OutPendingChunks, int32& OutTotalVoxels) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Terrain generator */
    UPROPERTY()
    TObjectPtr<UVoxelTerrainGenerator> TerrainGenerator;

    /** Map of loaded chunks */
    UPROPERTY()
    TMap<FChunkCoord, TObjectPtr<AVoxelChunk>> LoadedChunks;

    /** Queue of chunks waiting to be generated */
    TArray<FChunkCoord> ChunkGenerationQueue;

    /** Queue of chunks waiting for mesh building */
    TArray<AVoxelChunk*> MeshBuildQueue;

    /** Current load center in chunk coordinates */
    FChunkCoord CurrentLoadCenter;

    /** Is world initialized */
    bool bIsInitialized = false;

    /** Create and spawn a new chunk */
    AVoxelChunk* CreateChunk(const FChunkCoord& ChunkCoord);

    /** Destroy a chunk */
    void DestroyChunk(const FChunkCoord& ChunkCoord);

    /** Update which chunks should be loaded based on load center */
    void UpdateChunkLoading();

    /** Process chunk generation queue */
    void ProcessGenerationQueue();

    /** Process mesh build queue */
    void ProcessMeshBuildQueue();

    /** Update neighbor references for a chunk */
    void UpdateChunkNeighbors(AVoxelChunk* Chunk);

    /** Get distance from load center (in chunks) */
    float GetChunkDistanceFromCenter(const FChunkCoord& ChunkCoord) const;

    /** Sort chunks by distance from load center */
    void SortQueueByDistance(TArray<FChunkCoord>& Queue);
};
