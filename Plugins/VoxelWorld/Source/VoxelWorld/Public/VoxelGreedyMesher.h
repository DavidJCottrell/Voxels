// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelTypes.h"

struct FVoxelMeshData;

/**
 * Greedy meshing algorithm for optimized voxel mesh generation
 * Merges adjacent faces of the same type into larger quads
 */
class VOXELWORLD_API FVoxelGreedyMesher
{
public:
    FVoxelGreedyMesher(int32 InChunkSize, float InVoxelSize);

    /**
     * Generate optimized mesh data using greedy meshing
     * @param VoxelData Array of voxels in the chunk
     * @param GetNeighborVoxel Function to get voxels from neighboring chunks
     * @param OutMeshData Output mesh data
     */
    void GenerateMesh(
        const TArray<FVoxel>& VoxelData,
        TFunction<FVoxel(int32, int32, int32)> GetNeighborVoxel,
        FVoxelMeshData& OutMeshData
    );

private:
    int32 ChunkSize;
    float VoxelSize;

    /** Mask for tracking which faces have been meshed */
    TArray<bool> FaceMask;

    /** Get voxel index in array */
    FORCEINLINE int32 GetIndex(int32 X, int32 Y, int32 Z) const
    {
        return X + Y * ChunkSize + Z * ChunkSize * ChunkSize;
    }

    /** Process a single slice of the chunk for a given axis */
    void ProcessSlice(
        const TArray<FVoxel>& VoxelData,
        TFunction<FVoxel(int32, int32, int32)> GetNeighborVoxel,
        FVoxelMeshData& OutMeshData,
        int32 Axis,
        bool BackFace
    );

    /** Get color for voxel type */
    FColor GetVoxelColor(EVoxelType Type) const;

    /** Add a merged quad to mesh data */
    void AddQuad(
        FVoxelMeshData& OutMeshData,
        const FVector& Position,
        const FVector& DU,
        const FVector& DV,
        int32 Width,
        int32 Height,
        const FVector& Normal,
        EVoxelType VoxelType
    );
};
