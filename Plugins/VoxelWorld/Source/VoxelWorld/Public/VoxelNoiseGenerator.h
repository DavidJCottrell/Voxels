// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VoxelNoiseGenerator.generated.h"

/**
 * Noise generator class for procedural terrain generation
 * Implements Perlin noise and various fractal noise functions
 */
UCLASS(BlueprintType, Blueprintable)
class VOXELWORLD_API UVoxelNoiseGenerator : public UObject
{
    GENERATED_BODY()

public:
    UVoxelNoiseGenerator();

    /** Initialize the noise generator with a seed */
    UFUNCTION(BlueprintCallable, Category = "Noise")
    void Initialize(int32 Seed);

    /** Get 2D Perlin noise value */
    UFUNCTION(BlueprintCallable, Category = "Noise")
    float GetNoise2D(float X, float Y) const;

    /** Get 3D Perlin noise value */
    UFUNCTION(BlueprintCallable, Category = "Noise")
    float GetNoise3D(float X, float Y, float Z) const;

    /** Get fractal (octave) noise 2D */
    UFUNCTION(BlueprintCallable, Category = "Noise")
    float GetFractalNoise2D(float X, float Y, int32 Octaves, float Persistence = 0.5f, float Lacunarity = 2.0f) const;

    /** Get fractal (octave) noise 3D */
    UFUNCTION(BlueprintCallable, Category = "Noise")
    float GetFractalNoise3D(float X, float Y, float Z, int32 Octaves, float Persistence = 0.5f, float Lacunarity = 2.0f) const;

    /** Get ridged noise (for mountains) */
    UFUNCTION(BlueprintCallable, Category = "Noise")
    float GetRidgedNoise2D(float X, float Y, int32 Octaves, float Persistence = 0.5f, float Lacunarity = 2.0f) const;

    /** Get billow noise (for clouds/terrain) */
    UFUNCTION(BlueprintCallable, Category = "Noise")
    float GetBillowNoise2D(float X, float Y, int32 Octaves, float Persistence = 0.5f, float Lacunarity = 2.0f) const;

    /** Get Voronoi noise (for biomes) */
    UFUNCTION(BlueprintCallable, Category = "Noise")
    float GetVoronoiNoise2D(float X, float Y, float& OutCellX, float& OutCellY) const;

protected:
    /** Permutation table for noise generation */
    TArray<int32> Permutation;

    /** Gradient vectors for 3D noise */
    static const FVector GradientVectors3D[16];

    /** Fade function for smooth interpolation */
    FORCEINLINE float Fade(float T) const
    {
        return T * T * T * (T * (T * 6.0f - 15.0f) + 10.0f);
    }

    /** Linear interpolation */
    FORCEINLINE float Lerp(float A, float B, float T) const
    {
        return A + T * (B - A);
    }

    /** Gradient function for 2D */
    float Gradient2D(int32 Hash, float X, float Y) const;

    /** Gradient function for 3D */
    float Gradient3D(int32 Hash, float X, float Y, float Z) const;

    /** Hash function */
    FORCEINLINE int32 Hash(int32 X) const
    {
        return Permutation[X & 255];
    }
};
