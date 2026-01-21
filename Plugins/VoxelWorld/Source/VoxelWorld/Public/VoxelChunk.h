// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "VoxelTypes.h"
#include "VoxelMarchingCubes.h"
#include "VoxelChunk.generated.h"

class UVoxelTerrainGenerator;

/**
 * Represents a single chunk in the voxel world
 * Optimized with LOD support, distance-based collision, and memory management
 */
UCLASS()
class VOXELWORLD_API AVoxelChunk : public AActor
{
    GENERATED_BODY()

public:
    AVoxelChunk();
    ~AVoxelChunk();

    /** Initialize the chunk with coordinates and settings */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void InitializeChunk(const FChunkCoord& InChunkCoord, const FVoxelWorldSettings& InSettings, UVoxelTerrainGenerator* InGenerator);

    /** Reset chunk for pooling - clears data but keeps allocations */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void ResetChunk();

    /** Generate voxel data for this chunk */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void GenerateVoxelData();

    /** Build the mesh from voxel data with LOD support */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void BuildMesh();

    /** Build mesh with specific LOD level */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void BuildMeshWithLOD(EVoxelLOD LODLevel);

    /** Get voxel at local position */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FVoxel GetVoxel(int32 LocalX, int32 LocalY, int32 LocalZ) const;

    /** Set voxel at local position */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void SetVoxel(int32 LocalX, int32 LocalY, int32 LocalZ, const FVoxel& Voxel);

    /** Get density at local position */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    float GetDensity(int32 LocalX, int32 LocalY, int32 LocalZ) const;

    /** Set density at local position */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void SetDensity(int32 LocalX, int32 LocalY, int32 LocalZ, float Density);

    /** Modify terrain smoothly */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void ModifyTerrain(const FVector& LocalPosition, float Radius, float Strength, bool bAdd);

    /** Get material type at local position */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    EVoxelType GetMaterial(int32 LocalX, int32 LocalY, int32 LocalZ) const;

    /** Set material type at local position */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void SetMaterial(int32 LocalX, int32 LocalY, int32 LocalZ, EVoxelType Material);

    /** Get chunk coordinate */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FChunkCoord GetChunkCoord() const { return ChunkCoord; }

    /** Check if chunk needs mesh rebuild */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    bool NeedsMeshRebuild() const { return bNeedsMeshRebuild; }

    /** Mark chunk as needing mesh rebuild */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void MarkMeshDirty() { bNeedsMeshRebuild = true; }

    /** Check if chunk is fully generated */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    bool IsGenerated() const { return bIsGenerated; }

    /** Set neighbor chunks */
    void SetNeighbors(AVoxelChunk* XPos, AVoxelChunk* XNeg, AVoxelChunk* YPos, AVoxelChunk* YNeg, AVoxelChunk* ZPos, AVoxelChunk* ZNeg);

    /** Get density from this chunk or neighbors */
    float GetDensityIncludingNeighbors(int32 LocalX, int32 LocalY, int32 LocalZ) const;

    /** Get material from this chunk or neighbors */
    EVoxelType GetMaterialIncludingNeighbors(int32 LocalX, int32 LocalY, int32 LocalZ) const;

    // ==========================================
    // LOD and Performance
    // ==========================================

    /** Get current LOD level */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    EVoxelLOD GetCurrentLOD() const { return CurrentLOD; }

    /** Set LOD level - will trigger mesh rebuild if changed */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    void SetLOD(EVoxelLOD NewLOD);

    /** Enable or disable collision for this chunk */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    void SetCollisionEnabled(bool bEnabled);

    /** Check if collision is enabled */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    bool IsCollisionEnabled() const { return bCollisionEnabled; }

    /** Unload voxel data to free memory (mesh remains) */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    void UnloadVoxelData();

    /** Check if voxel data is loaded */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    bool HasVoxelData() const { return bHasVoxelData; }

    /** Reload voxel data (regenerates from terrain generator) */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    void ReloadVoxelData();

    /** Get memory usage in bytes */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    int64 GetMemoryUsage() const;

    /** Get chunk state */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    EChunkState GetChunkState() const { return ChunkState; }

    /** Set chunk state */
    void SetChunkState(EChunkState NewState) { ChunkState = NewState; }

    /** Mark chunk for cancellation (async safety) */
    void MarkPendingKill() { bPendingKill = true; }
    bool IsPendingKillOrUnreachable() const { return bPendingKill || !IsValidLowLevel(); }

    /** Compact memory - shrink arrays to actual size */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    void CompactMemory();

protected:
    virtual void BeginPlay() override;
    virtual void BeginDestroy() override;

    /** Procedural mesh component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    /** Chunk coordinate in chunk space */
    UPROPERTY()
    FChunkCoord ChunkCoord;

    /** World settings */
    FVoxelWorldSettings WorldSettings;

    /** Terrain generator reference */
    UPROPERTY()
    TObjectPtr<UVoxelTerrainGenerator> TerrainGenerator;

    /** Density data for smooth terrain (SDF) */
    TArray<float> DensityData;

    /** Material data */
    TArray<EVoxelType> MaterialData;

    /** Marching cubes mesher instance */
    TUniquePtr<FVoxelMarchingCubes> MarchingCubes;

    /** Neighbor chunk references */
    TWeakObjectPtr<AVoxelChunk> NeighborXPos;
    TWeakObjectPtr<AVoxelChunk> NeighborXNeg;
    TWeakObjectPtr<AVoxelChunk> NeighborYPos;
    TWeakObjectPtr<AVoxelChunk> NeighborYNeg;
    TWeakObjectPtr<AVoxelChunk> NeighborZPos;
    TWeakObjectPtr<AVoxelChunk> NeighborZNeg;

    /** Current LOD level */
    EVoxelLOD CurrentLOD = EVoxelLOD::LOD0;

    /** Current chunk state */
    EChunkState ChunkState = EChunkState::Unloaded;

    /** State flags */
    bool bIsGenerated = false;
    bool bNeedsMeshRebuild = false;
    bool bCollisionEnabled = true;
    bool bHasVoxelData = false;

    /** Thread safety flag for async operations */
    TAtomic<bool> bPendingKill{false};

    /** Convert local coordinates to density array index */
    FORCEINLINE int32 GetDensityIndex(int32 X, int32 Y, int32 Z) const
    {
        int32 Size = WorldSettings.ChunkSize + 1;
        return X + Y * Size + Z * Size * Size;
    }

    /** Convert local coordinates to material array index */
    FORCEINLINE int32 GetMaterialIndex(int32 X, int32 Y, int32 Z) const
    {
        return X + Y * WorldSettings.ChunkSize + Z * WorldSettings.ChunkSize * WorldSettings.ChunkSize;
    }

    /** Check if local coordinates are within chunk bounds */
    FORCEINLINE bool IsInBounds(int32 X, int32 Y, int32 Z) const
    {
        return X >= 0 && X < WorldSettings.ChunkSize &&
               Y >= 0 && Y < WorldSettings.ChunkSize &&
               Z >= 0 && Z < WorldSettings.ChunkSize;
    }

    /** Check if coordinates are valid for density grid */
    FORCEINLINE bool IsInDensityBounds(int32 X, int32 Y, int32 Z) const
    {
        return X >= 0 && X <= WorldSettings.ChunkSize &&
               Y >= 0 && Y <= WorldSettings.ChunkSize &&
               Z >= 0 && Z <= WorldSettings.ChunkSize;
    }

    /** Get color for voxel type */
    FColor GetVoxelColor(EVoxelType Type) const;

private:
    /** Clear mesh data */
    void ClearMesh();
};
