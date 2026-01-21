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

protected:
    /** Noise generator instance */
    UPROPERTY()
    TObjectPtr<UVoxelNoiseGenerator> NoiseGenerator;

    /** Cached world settings */
    FVoxelWorldSettings WorldSettings;

    /** Get continentalness (land vs ocean) - affects base terrain height */
    float GetContinentalness(int32 WorldX, int32 WorldY) const;

    /** Get erosion factor - affects terrain smoothness */
    float GetErosion(int32 WorldX, int32 WorldY) const;

    /** Get peaks and valleys factor - creates mountain ridges */
    float GetPeaksValleys(int32 WorldX, int32 WorldY) const;

    /** Get 3D terrain density variation for overhangs and caves */
    float Get3DTerrainVariation(int32 WorldX, int32 WorldY, int32 WorldZ) const;

    /** Determine surface material based on biome and conditions */
    EVoxelType GetSurfaceBlock(EBiomeType Biome, int32 WorldX, int32 WorldY, int32 WorldZ, float TerrainHeight) const;

    /** Determine underground material based on depth and conditions */
    EVoxelType GetUndergroundBlock(int32 WorldZ, float TerrainHeight, EBiomeType Biome) const;
};
