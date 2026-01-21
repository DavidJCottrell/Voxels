// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "VoxelTypes.h"
#include "VoxelChunk.generated.h"

class UVoxelTerrainGenerator;

/** Mesh data structure for chunk generation */
USTRUCT()
struct FVoxelMeshData
{
    GENERATED_BODY()

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    void Reset()
    {
        Vertices.Empty();
        Triangles.Empty();
        Normals.Empty();
        UVs.Empty();
        VertexColors.Empty();
        Tangents.Empty();
    }

    bool IsEmpty() const
    {
        return Vertices.Num() == 0;
    }
};

/**
 * Represents a single chunk in the voxel world
 * Handles voxel storage and mesh generation
 */
UCLASS()
class VOXELWORLD_API AVoxelChunk : public AActor
{
    GENERATED_BODY()

public:
    AVoxelChunk();

    /** Initialize the chunk with coordinates and settings */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void InitializeChunk(const FChunkCoord& InChunkCoord, const FVoxelWorldSettings& InSettings, UVoxelTerrainGenerator* InGenerator);

    /** Generate voxel data for this chunk */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void GenerateVoxelData();

    /** Build the mesh from voxel data */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void BuildMesh();

    /** Get voxel at local position */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FVoxel GetVoxel(int32 LocalX, int32 LocalY, int32 LocalZ) const;

    /** Set voxel at local position */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void SetVoxel(int32 LocalX, int32 LocalY, int32 LocalZ, const FVoxel& Voxel);

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

    /** Set neighbor chunks for seamless mesh generation */
    void SetNeighbors(AVoxelChunk* XPos, AVoxelChunk* XNeg, AVoxelChunk* YPos, AVoxelChunk* YNeg, AVoxelChunk* ZPos, AVoxelChunk* ZNeg);

protected:
    virtual void BeginPlay() override;

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

    /** Voxel data storage */
    TArray<FVoxel> VoxelData;

    /** Neighbor chunk references */
    TWeakObjectPtr<AVoxelChunk> NeighborXPos;
    TWeakObjectPtr<AVoxelChunk> NeighborXNeg;
    TWeakObjectPtr<AVoxelChunk> NeighborYPos;
    TWeakObjectPtr<AVoxelChunk> NeighborYNeg;
    TWeakObjectPtr<AVoxelChunk> NeighborZPos;
    TWeakObjectPtr<AVoxelChunk> NeighborZNeg;

    /** State flags */
    bool bIsGenerated = false;
    bool bNeedsMeshRebuild = false;

    /** Convert local coordinates to array index */
    FORCEINLINE int32 GetVoxelIndex(int32 X, int32 Y, int32 Z) const
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

    /** Get voxel from this chunk or neighbor */
    FVoxel GetVoxelIncludingNeighbors(int32 LocalX, int32 LocalY, int32 LocalZ) const;

    /** Check if a face should be rendered (adjacent voxel is transparent) */
    bool ShouldRenderFace(int32 LocalX, int32 LocalY, int32 LocalZ, int32 DirX, int32 DirY, int32 DirZ) const;

    /** Add a face to mesh data */
    void AddFace(FVoxelMeshData& MeshData, const FVector& Position, int32 FaceIndex, EVoxelType VoxelType) const;

    /** Get color for voxel type */
    FColor GetVoxelColor(EVoxelType Type) const;
};
