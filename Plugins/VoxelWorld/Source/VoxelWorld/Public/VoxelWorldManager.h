// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelTypes.h"
#include "HAL/ThreadSafeBool.h"
#include "VoxelWorldManager.generated.h"

class AVoxelChunk;
class UVoxelTerrainGenerator;

/** Async task for chunk generation - with safe cancellation */
class FChunkGenerationTask : public FNonAbandonableTask
{
public:
    FChunkGenerationTask(AVoxelChunk* InChunk, FThreadSafeBool* InCancelFlag)
        : Chunk(InChunk)
        , CancelFlag(InCancelFlag)
    {
    }

    FORCEINLINE TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FChunkGenerationTask, STATGROUP_ThreadPoolAsyncTasks);
    }

    void DoWork();

private:
    AVoxelChunk* Chunk;
    FThreadSafeBool* CancelFlag;
};

/**
 * Main voxel world manager with LOD, collision distance, and memory management
 */
UCLASS()
class VOXELWORLD_API AVoxelWorldManager : public AActor
{
    GENERATED_BODY()

public:
    AVoxelWorldManager();
    virtual ~AVoxelWorldManager();

    virtual void Tick(float DeltaTime) override;

    /** World generation settings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel World")
    FVoxelWorldSettings WorldSettings;

    /** Material to apply to voxel meshes */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel World")
    TObjectPtr<UMaterialInterface> VoxelMaterial;

    // ==========================================
    // Editor Preview Settings
    // ==========================================

    /** Enable real-time preview in editor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview")
    bool bEnableEditorPreview = false;

    /** Preview render distance (keep small for editor performance) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview", meta = (ClampMin = "1", ClampMax = "8", EditCondition = "bEnableEditorPreview"))
    int32 EditorPreviewDistance = 3;

    /** Auto-regenerate preview when settings change */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview", meta = (EditCondition = "bEnableEditorPreview"))
    bool bAutoRegeneratePreview = true;

    /** Manually regenerate the editor preview */
    UFUNCTION(CallInEditor, Category = "Editor Preview")
    void RegenerateEditorPreview();

    /** Clear all preview chunks */
    UFUNCTION(CallInEditor, Category = "Editor Preview")
    void ClearEditorPreview();

    // ==========================================
    // Runtime Functions
    // ==========================================

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

    // ==========================================
    // Performance Monitoring
    // ==========================================

    /** Get total memory usage in MB */
    UFUNCTION(BlueprintCallable, Category = "Voxel World|Performance")
    float GetMemoryUsageMB() const;

    /** Get number of chunks in pool */
    UFUNCTION(BlueprintCallable, Category = "Voxel World|Performance")
    int32 GetPooledChunkCount() const { return ChunkPool.Num(); }

    /** Force garbage collection of unused chunks and memory compaction */
    UFUNCTION(BlueprintCallable, Category = "Voxel World|Performance")
    void ForceCleanup();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual bool ShouldTickIfViewportsOnly() const override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    /** Terrain generator */
    UPROPERTY()
    TObjectPtr<UVoxelTerrainGenerator> TerrainGenerator;

    /** Map of loaded chunks */
    UPROPERTY()
    TMap<FChunkCoord, TObjectPtr<AVoxelChunk>> LoadedChunks;

    /** Pool of reusable chunks */
    UPROPERTY()
    TArray<TObjectPtr<AVoxelChunk>> ChunkPool;

    /** Queue of chunks waiting to be generated */
    TArray<FChunkCoord> ChunkGenerationQueue;

    /** Queue of chunks waiting for mesh building */
    TArray<AVoxelChunk*> MeshBuildQueue;

    /** Current load center in chunk coordinates */
    FChunkCoord CurrentLoadCenter;

    /** Cancel flag for async tasks - prevents memory leak when stopping */
    FThreadSafeBool bCancelAsyncTasks;

    /** Timers for periodic updates */
    float LODUpdateTimer = 0.0f;
    float CollisionUpdateTimer = 0.0f;

    /** Update intervals */
    static constexpr float LODUpdateInterval = 0.5f;
    static constexpr float CollisionUpdateInterval = 0.5f;

    /** Is world initialized */
    bool bIsInitialized = false;

    /** Is this an editor preview */
    bool bIsEditorPreview = false;

    // ==========================================
    // Chunk Management
    // ==========================================

    /** Create or get chunk from pool */
    AVoxelChunk* CreateOrGetChunk(const FChunkCoord& ChunkCoord);

    /** Return chunk to pool or destroy it */
    void RecycleChunk(const FChunkCoord& ChunkCoord);

    /** Destroy a chunk completely (not pooled) */
    void DestroyChunk(const FChunkCoord& ChunkCoord);

    /** Destroy all chunks and clear pools */
    void DestroyAllChunks();

    /** Update chunk loading/unloading based on distance */
    void UpdateChunkLoading();

    /** Update LOD levels for all chunks based on distance */
    void UpdateChunkLODs();

    /** Update collision states for all chunks based on distance */
    void UpdateChunkCollisions();

    /** Process chunk generation queue */
    void ProcessGenerationQueue();

    /** Process mesh build queue */
    void ProcessMeshBuildQueue();

    /** Update neighbor references for a chunk */
    void UpdateChunkNeighbors(AVoxelChunk* Chunk);

    // ==========================================
    // Distance Calculations
    // ==========================================

    /** Get distance from load center (in chunks) - horizontal only */
    float GetChunkDistanceFromCenter(const FChunkCoord& ChunkCoord) const;

    /** Get LOD level for a chunk based on distance */
    EVoxelLOD GetLODForDistance(float Distance) const;

    /** Sort chunks by distance from load center */
    void SortQueueByDistance(TArray<FChunkCoord>& Queue);

    /** Get effective render distance (editor vs runtime) */
    int32 GetEffectiveRenderDistance() const;

    // ==========================================
    // Async Task Management
    // ==========================================

    /** Wait for all async tasks to complete (called during shutdown) */
    void WaitForAsyncTasks();

    /** Counter for active async tasks */
    TAtomic<int32> ActiveAsyncTasks{0};

#if WITH_EDITOR
    void InitializeEditorPreview();
    bool IsInEditorPreviewMode() const;
#endif
};
