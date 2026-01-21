// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VoxelTypes.h"
#include "VoxelTerrainGenerator.generated.h"

class UVoxelNoiseGenerator;

/**
 * Terrain generator for voxel worlds
 * Handles biome distribution, terrain shaping, and feature placement
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

    /** Generate terrain height at a given world position */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    int32 GetTerrainHeight(int32 WorldX, int32 WorldY) const;

    /** Get the biome at a given position */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    EBiomeType GetBiome(int32 WorldX, int32 WorldY) const;

    /** Get the voxel type at a given world position */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    EVoxelType GetVoxelType(int32 WorldX, int32 WorldY, int32 WorldZ) const;

    /** Check if there should be a cave at this position */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    bool IsCave(int32 WorldX, int32 WorldY, int32 WorldZ) const;

    /** Generate an entire chunk's voxel data */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void GenerateChunkData(const FChunkCoord& ChunkCoord, TArray<FVoxel>& OutVoxelData) const;

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

    /** Get continentalness (land vs ocean) */
    float GetContinentalness(int32 WorldX, int32 WorldY) const;

    /** Get erosion factor */
    float GetErosion(int32 WorldX, int32 WorldY) const;

    /** Get peaks and valleys factor */
    float GetPeaksValleys(int32 WorldX, int32 WorldY) const;

    /** Determine surface block based on biome and conditions */
    EVoxelType GetSurfaceBlock(EBiomeType Biome, int32 WorldX, int32 WorldY, int32 WorldZ, int32 TerrainHeight) const;

    /** Determine underground block based on depth and conditions */
    EVoxelType GetUndergroundBlock(int32 WorldZ, int32 TerrainHeight, EBiomeType Biome) const;
};
