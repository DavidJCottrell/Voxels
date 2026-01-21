// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelTypes.h"

/**
 * Marching Cubes implementation with LOD support and vertex deduplication
 */
class VOXELWORLD_API FVoxelMarchingCubes
{
public:
    FVoxelMarchingCubes(int32 InChunkSize, float InVoxelSize, float InSurfaceLevel = 0.0f);

    /**
     * Generate mesh with LOD support and optional vertex deduplication
     * @param StepSize LOD step size (1=full, 2=half, 4=quarter, 8=eighth detail)
     * @param bDeduplicateVertices Whether to share vertices between triangles
     */
    void GenerateMeshLOD(
        const TArray<float>& DensityData,
        const TArray<EVoxelType>& MaterialData,
        TFunction<float(int32, int32, int32)> GetNeighborDensity,
        TFunction<EVoxelType(int32, int32, int32)> GetNeighborMaterial,
        FVoxelMeshData& OutMeshData,
        int32 StepSize = 1,
        bool bDeduplicateVertices = true
    );

    /** Legacy method - calls GenerateMeshLOD with step size 1 */
    void GenerateMesh(
        const TArray<float>& DensityData,
        const TArray<EVoxelType>& MaterialData,
        TFunction<float(int32, int32, int32)> GetNeighborDensity,
        TFunction<EVoxelType(int32, int32, int32)> GetNeighborMaterial,
        FVoxelMeshData& OutMeshData
    );

    void SetSurfaceLevel(float NewSurfaceLevel) { SurfaceLevel = NewSurfaceLevel; }

private:
    int32 ChunkSize;
    float VoxelSize;
    float SurfaceLevel;

    /** Vertex deduplication map - maps position hash to vertex index */
    TMap<uint64, int32> VertexMap;

    FORCEINLINE int32 GetIndex(int32 X, int32 Y, int32 Z) const
    {
        return X + Y * (ChunkSize + 1) + Z * (ChunkSize + 1) * (ChunkSize + 1);
    }

    /** Hash a position for vertex deduplication */
    FORCEINLINE uint64 HashPosition(const FVector& Position) const
    {
        // Quantize position to avoid floating point precision issues
        int32 QX = FMath::RoundToInt(Position.X * 100.0f);
        int32 QY = FMath::RoundToInt(Position.Y * 100.0f);
        int32 QZ = FMath::RoundToInt(Position.Z * 100.0f);

        // Combine into 64-bit hash
        return (static_cast<uint64>(QX & 0x1FFFFF) << 42) |
               (static_cast<uint64>(QY & 0x1FFFFF) << 21) |
               (static_cast<uint64>(QZ & 0x1FFFFF));
    }

    /** Add vertex with deduplication */
    int32 AddVertex(
        FVoxelMeshData& OutMeshData,
        const FVector& Position,
        const FVector& Normal,
        const FVector2D& UV,
        const FColor& Color,
        const FVector& Tangent,
        bool bDeduplicate
    );

    FVector InterpolateVertex(
        const FVector& P1, const FVector& P2,
        float D1, float D2
    ) const;

    FVector CalculateNormal(
        const TArray<float>& DensityData,
        TFunction<float(int32, int32, int32)> GetNeighborDensity,
        int32 X, int32 Y, int32 Z,
        int32 StepSize
    ) const;

    float GetDensity(
        const TArray<float>& DensityData,
        TFunction<float(int32, int32, int32)> GetNeighborDensity,
        int32 X, int32 Y, int32 Z
    ) const;

    FColor GetVoxelColor(EVoxelType Type) const;

    EVoxelType GetDominantMaterial(
        const TArray<EVoxelType>& MaterialData,
        TFunction<EVoxelType(int32, int32, int32)> GetNeighborMaterial,
        int32 X, int32 Y, int32 Z
    ) const;

    // Marching Cubes Lookup Tables
    static const int32 EdgeTable[256];
    static const int32 TriangleTable[256][16];
    static const FIntVector CornerOffsets[8];
    static const int32 EdgeConnections[12][2];
};
