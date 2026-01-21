// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VoxelTypes.h"
#include "VoxelTerrainGenerator.generated.h"

class UVoxelNoiseGenerator;

/**
 * Terrain generator for voxel worlds using Signed Distance Fields
 * Produces smooth terrain data for Marching Cubes mesh generation
 * Features: Plateaus, Deep Valleys, Canyons, and varied biomes
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VOXELWORLD_API UVoxelTerrainGenerator : public UObject
{
    GENERATED_BODY()

public:
    UVoxelTerrainGenerator();

    /** Initialize the terrain generator */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void Initialize(const FVoxelWorldSettings& Settings);

    /**
     * Get density value at world position (Signed Distance Field)
     * Negative = inside terrain (solid)
     * Positive = outside terrain (air)
     * Zero = exactly on surface
     */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    float GetDensity(int32 WorldX, int32 WorldY, int32 WorldZ) const;

    /** Generate terrain height at a given world position */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    float GetTerrainHeight(int32 WorldX, int32 WorldY) const;

    /** Get the biome at a given position */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    EBiomeType GetBiome(int32 WorldX, int32 WorldY) const;

    /** Get the voxel/material type at a given world position */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    EVoxelType GetVoxelType(int32 WorldX, int32 WorldY, int32 WorldZ) const;

    /** Check if there should be a cave at this position */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    bool IsCave(int32 WorldX, int32 WorldY, int32 WorldZ) const;

    /** Get cave density (for smooth cave walls) */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    float GetCaveDensity(int32 WorldX, int32 WorldY, int32 WorldZ) const;

    /** Get temperature value at position (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    float GetTemperature(int32 WorldX, int32 WorldY) const;

    /** Get moisture value at position (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    float GetMoisture(int32 WorldX, int32 WorldY) const;

    // ==========================================
    // Biome Feature Queries
    // ==========================================

    /** Get the terrain feature type (plateau, valley, etc.) */
    UFUNCTION(BlueprintCallable, Category = "Terrain|Biome")
    EBiomeType GetTerrainFeature(int32 WorldX, int32 WorldY) const;

    /** Get plateau influence at position (0-1, 0 = no plateau, 1 = full plateau) */
    UFUNCTION(BlueprintCallable, Category = "Terrain|Biome")
    float GetPlateauInfluence(int32 WorldX, int32 WorldY) const;

    /** Get valley influence at position (0-1, 0 = no valley, 1 = deep valley) */
    UFUNCTION(BlueprintCallable, Category = "Terrain|Biome")
    float GetValleyInfluence(int32 WorldX, int32 WorldY) const;

    /** Get canyon influence at position */
    UFUNCTION(BlueprintCallable, Category = "Terrain|Biome")
    float GetCanyonInfluence(int32 WorldX, int32 WorldY) const;

    /** Check if a chunk is likely to be empty (for optimization) */
    UFUNCTION(BlueprintCallable, Category = "Terrain|Optimization")
    bool IsChunkLikelyEmpty(int32 ChunkX, int32 ChunkY, int32 ChunkZ, int32 ChunkSize) const;

protected:
    /** Noise generator instance */
    UPROPERTY()
    TObjectPtr<UVoxelNoiseGenerator> NoiseGenerator;

    /** Cached world settings */
    FVoxelWorldSettings WorldSettings;

    // ==========================================
    // Base Terrain Generation
    // ==========================================

    /** Get continentalness (land vs ocean) - affects base terrain height */
    float GetContinentalness(int32 WorldX, int32 WorldY) const;

    /** Get erosion factor - affects terrain smoothness */
    float GetErosion(int32 WorldX, int32 WorldY) const;

    /** Get peaks and valleys factor - creates mountain ridges */
    float GetPeaksValleys(int32 WorldX, int32 WorldY) const;

    /** Get 3D terrain density variation for overhangs and caves */
    float Get3DTerrainVariation(int32 WorldX, int32 WorldY, int32 WorldZ) const;

    // ==========================================
    // Biome Feature Generation
    // ==========================================

    /** Get the base height for terrain features (plateaus, valleys) */
    float GetFeatureHeight(int32 WorldX, int32 WorldY) const;

    /** Generate plateau terrain - flat-topped mountains */
    float GetPlateauDensity(int32 WorldX, int32 WorldY, int32 WorldZ, float BaseHeight) const;

    /** Generate deep valley terrain */
    float GetValleyDensity(int32 WorldX, int32 WorldY, int32 WorldZ, float BaseHeight) const;

    /** Generate canyon features */
    float GetCanyonDensity(int32 WorldX, int32 WorldY, int32 WorldZ, float BaseHeight) const;

    /** Blend between different terrain features */
    float BlendTerrainFeatures(int32 WorldX, int32 WorldY, int32 WorldZ) const;

    // ==========================================
    // Material Selection
    // ==========================================

    /** Determine surface material based on biome and conditions */
    EVoxelType GetSurfaceBlock(EBiomeType Biome, int32 WorldX, int32 WorldY, int32 WorldZ, float TerrainHeight) const;

    /** Determine underground material based on depth and conditions */
    EVoxelType GetUndergroundBlock(int32 WorldZ, float TerrainHeight, EBiomeType Biome) const;

    /** Get material for plateau surfaces */
    EVoxelType GetPlateauMaterial(int32 WorldX, int32 WorldY, int32 WorldZ, float TerrainHeight) const;

    /** Get material for valley floors */
    EVoxelType GetValleyMaterial(int32 WorldX, int32 WorldY, int32 WorldZ, float TerrainHeight) const;

private:
    /** Cached noise values for biome determination (performance optimization) */
    mutable TMap<uint64, float> CachedBiomeNoise;

    /** Hash function for position caching */
    FORCEINLINE uint64 HashPosition2D(int32 X, int32 Y) const
    {
        // Quantize to reduce cache size while maintaining quality
        int32 QX = X >> 2; // Divide by 4
        int32 QY = Y >> 2;
        return (static_cast<uint64>(QX & 0xFFFFFF) << 24) | static_cast<uint64>(QY & 0xFFFFFF);
    }
};
