// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelTypes.h"

/**
 * Marching Cubes implementation for smooth voxel terrain
 * Generates smooth meshes by interpolating between density values
 */
class VOXELWORLD_API FVoxelMarchingCubes
{
public:
    FVoxelMarchingCubes(int32 InChunkSize, float InVoxelSize, float InSurfaceLevel = 0.0f);

    /**
     * Generate smooth mesh using Marching Cubes algorithm
     * @param DensityData Array of density values (-1 to 1, where < 0 is solid)
     * @param MaterialData Array of material types for coloring
     * @param GetNeighborDensity Function to get density from neighboring chunks
     * @param GetNeighborMaterial Function to get material from neighboring chunks
     * @param OutMeshData Output mesh data
     */
    void GenerateMesh(
        const TArray<float>& DensityData,
        const TArray<EVoxelType>& MaterialData,
        TFunction<float(int32, int32, int32)> GetNeighborDensity,
        TFunction<EVoxelType(int32, int32, int32)> GetNeighborMaterial,
        FVoxelMeshData& OutMeshData
    );

    /** Set the surface level threshold (default 0.0) */
    void SetSurfaceLevel(float NewSurfaceLevel) { SurfaceLevel = NewSurfaceLevel; }

private:
    int32 ChunkSize;
    float VoxelSize;
    float SurfaceLevel;

    /** Get index in density array */
    FORCEINLINE int32 GetIndex(int32 X, int32 Y, int32 Z) const
    {
        return X + Y * (ChunkSize + 1) + Z * (ChunkSize + 1) * (ChunkSize + 1);
    }

    /** Interpolate vertex position along edge based on density values */
    FVector InterpolateVertex(
        const FVector& P1, const FVector& P2,
        float D1, float D2
    ) const;

    /** Calculate normal at a point using gradient of density field */
    FVector CalculateNormal(
        const TArray<float>& DensityData,
        TFunction<float(int32, int32, int32)> GetNeighborDensity,
        int32 X, int32 Y, int32 Z
    ) const;

    /** Get density value, handling boundary conditions */
    float GetDensity(
        const TArray<float>& DensityData,
        TFunction<float(int32, int32, int32)> GetNeighborDensity,
        int32 X, int32 Y, int32 Z
    ) const;

    /** Get color for voxel type */
    FColor GetVoxelColor(EVoxelType Type) const;

    /** Determine dominant material for a cell */
    EVoxelType GetDominantMaterial(
        const TArray<EVoxelType>& MaterialData,
        TFunction<EVoxelType(int32, int32, int32)> GetNeighborMaterial,
        int32 X, int32 Y, int32 Z
    ) const;

    // ============================================
    // Marching Cubes Lookup Tables
    // ============================================

    /** Edge table - which edges are intersected for each cube configuration */
    static const int32 EdgeTable[256];

    /** Triangle table - which triangles to generate for each configuration */
    static const int32 TriangleTable[256][16];

    /** Vertex offsets for cube corners */
    static const FIntVector CornerOffsets[8];

    /** Edge vertex indices (which two corners each edge connects) */
    static const int32 EdgeConnections[12][2];
};
