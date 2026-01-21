// Copyright Your Company. All Rights Reserved.

#include "VoxelTerrainGenerator.h"
#include "VoxelNoiseGenerator.h"
#include "VoxelWorldModule.h"

UVoxelTerrainGenerator::UVoxelTerrainGenerator()
{
    NoiseGenerator = nullptr;
}

void UVoxelTerrainGenerator::Initialize(const FVoxelWorldSettings& Settings)
{
    WorldSettings = Settings;
    
    NoiseGenerator = NewObject<UVoxelNoiseGenerator>(this);
    NoiseGenerator->Initialize(Settings.Seed);
    
    UE_LOG(LogVoxelWorld, Log, TEXT("Terrain generator initialized with seed: %d"), Settings.Seed);
}

float UVoxelTerrainGenerator::GetContinentalness(int32 WorldX, int32 WorldY) const
{
    if (!NoiseGenerator) return 0.5f;
    
    float Frequency = WorldSettings.NoiseFrequency * 0.3f;
    return NoiseGenerator->GetFractalNoise2D(
        WorldX * Frequency,
        WorldY * Frequency,
        3, 0.5f, 2.0f
    );
}

float UVoxelTerrainGenerator::GetErosion(int32 WorldX, int32 WorldY) const
{
    if (!NoiseGenerator) return 0.5f;
    
    float Frequency = WorldSettings.NoiseFrequency * 0.5f;
    return NoiseGenerator->GetFractalNoise2D(
        WorldX * Frequency + 1000.0f,
        WorldY * Frequency + 1000.0f,
        4, 0.5f, 2.0f
    );
}

float UVoxelTerrainGenerator::GetPeaksValleys(int32 WorldX, int32 WorldY) const
{
    if (!NoiseGenerator) return 0.5f;
    
    float Frequency = WorldSettings.NoiseFrequency * 2.0f;
    return NoiseGenerator->GetRidgedNoise2D(
        WorldX * Frequency + 2000.0f,
        WorldY * Frequency + 2000.0f,
        4, 0.5f, 2.0f
    );
}

float UVoxelTerrainGenerator::GetTemperature(int32 WorldX, int32 WorldY) const
{
    if (!NoiseGenerator) return 0.5f;
    
    float Frequency = WorldSettings.NoiseFrequency * 0.2f;
    return NoiseGenerator->GetFractalNoise2D(
        WorldX * Frequency + 5000.0f,
        WorldY * Frequency + 5000.0f,
        2, 0.5f, 2.0f
    );
}

float UVoxelTerrainGenerator::GetMoisture(int32 WorldX, int32 WorldY) const
{
    if (!NoiseGenerator) return 0.5f;
    
    float Frequency = WorldSettings.NoiseFrequency * 0.25f;
    return NoiseGenerator->GetFractalNoise2D(
        WorldX * Frequency + 7000.0f,
        WorldY * Frequency + 7000.0f,
        2, 0.5f, 2.0f
    );
}

float UVoxelTerrainGenerator::Get3DTerrainVariation(int32 WorldX, int32 WorldY, int32 WorldZ) const
{
    if (!NoiseGenerator) return 0.0f;
    
    // 3D noise for overhangs and terrain variation
    float Frequency = WorldSettings.NoiseFrequency * 1.5f;
    float Variation = NoiseGenerator->GetFractalNoise3D(
        WorldX * Frequency + 4000.0f,
        WorldY * Frequency + 4000.0f,
        WorldZ * Frequency + 4000.0f,
        3, 0.5f, 2.0f
    );
    
    // Convert from 0-1 to -1 to 1 range and scale
    return (Variation - 0.5f) * 8.0f;
}

EBiomeType UVoxelTerrainGenerator::GetBiome(int32 WorldX, int32 WorldY) const
{
    float Temperature = GetTemperature(WorldX, WorldY);
    float Moisture = GetMoisture(WorldX, WorldY);
    float Continentalness = GetContinentalness(WorldX, WorldY);
    
    // Ocean
    if (Continentalness < 0.3f)
    {
        return EBiomeType::Ocean;
    }
    
    // Land biomes based on temperature and moisture
    if (Temperature < 0.25f)
    {
        return EBiomeType::Tundra;
    }
    else if (Temperature > 0.75f)
    {
        if (Moisture < 0.3f)
        {
            return EBiomeType::Desert;
        }
        else if (Moisture > 0.7f)
        {
            return EBiomeType::Swamp;
        }
    }
    
    // Check for mountains based on erosion
    float Erosion = GetErosion(WorldX, WorldY);
    if (Erosion < 0.3f)
    {
        return EBiomeType::Mountains;
    }
    
    // Default biomes
    if (Moisture > 0.5f)
    {
        return EBiomeType::Forest;
    }
    
    return EBiomeType::Plains;
}

float UVoxelTerrainGenerator::GetTerrainHeight(int32 WorldX, int32 WorldY) const
{
    if (!NoiseGenerator) return WorldSettings.BaseTerrainHeight;
    
    // Get base terrain factors
    float Continentalness = GetContinentalness(WorldX, WorldY);
    float Erosion = GetErosion(WorldX, WorldY);
    float PeaksValleys = GetPeaksValleys(WorldX, WorldY);
    
    // Calculate base height
    float BaseHeight = WorldSettings.BaseTerrainHeight;
    
    // Apply continentalness (ocean vs land)
    float ContinentHeight = FMath::Lerp(-20.0f, 20.0f, Continentalness);
    
    // Apply erosion (smoothness vs roughness)
    float ErosionMultiplier = FMath::Lerp(0.3f, 1.0f, 1.0f - Erosion);
    
    // Apply peaks and valleys
    float TerrainVariation = (PeaksValleys - 0.5f) * WorldSettings.TerrainAmplitude * ErosionMultiplier;
    
    // Add detail noise
    float DetailNoise = NoiseGenerator->GetFractalNoise2D(
        WorldX * WorldSettings.NoiseFrequency * 4.0f,
        WorldY * WorldSettings.NoiseFrequency * 4.0f,
        WorldSettings.NoiseOctaves,
        0.5f, 2.0f
    );
    float DetailVariation = (DetailNoise - 0.5f) * 8.0f;
    
    // Combine all factors
    float FinalHeight = BaseHeight + ContinentHeight + TerrainVariation + DetailVariation;
    
    return FinalHeight;
}

float UVoxelTerrainGenerator::GetCaveDensity(int32 WorldX, int32 WorldY, int32 WorldZ) const
{
    if (!WorldSettings.bGenerateCaves || !NoiseGenerator) return 1.0f; // No cave = air
    
    float TerrainHeight = GetTerrainHeight(WorldX, WorldY);
    
    // Don't generate caves near surface or at bedrock
    if (WorldZ > TerrainHeight - 5 || WorldZ < 3) return 1.0f;
    
    // Use 3D noise for cave generation
    float CaveFrequency = WorldSettings.NoiseFrequency * 3.0f;
    float CaveNoise = NoiseGenerator->GetFractalNoise3D(
        WorldX * CaveFrequency,
        WorldY * CaveFrequency,
        WorldZ * CaveFrequency,
        3, 0.5f, 2.0f
    );
    
    // Secondary noise for more varied caves (worm-like tunnels)
    float CaveNoise2 = NoiseGenerator->GetFractalNoise3D(
        WorldX * CaveFrequency * 0.5f + 3000.0f,
        WorldY * CaveFrequency * 0.5f + 3000.0f,
        WorldZ * CaveFrequency * 0.5f + 3000.0f,
        2, 0.5f, 2.0f
    );
    
    // Combine noise values
    float CombinedNoise = (CaveNoise + CaveNoise2) * 0.5f;
    
    // Depth-based threshold (more caves deeper underground)
    float DepthFactor = 1.0f - (float)(WorldZ) / TerrainHeight;
    float AdjustedThreshold = WorldSettings.CaveThreshold - DepthFactor * 0.1f;
    
    // Convert to density value
    // If noise > threshold, we have a cave (return positive = air)
    // Smooth the transition for nice cave walls
    float CaveDensity = (CombinedNoise - AdjustedThreshold) * 5.0f;
    
    return CaveDensity;
}

bool UVoxelTerrainGenerator::IsCave(int32 WorldX, int32 WorldY, int32 WorldZ) const
{
    return GetCaveDensity(WorldX, WorldY, WorldZ) > 0.0f;
}

float UVoxelTerrainGenerator::GetDensity(int32 WorldX, int32 WorldY, int32 WorldZ) const
{
    // Get terrain height at this XY position
    float TerrainHeight = GetTerrainHeight(WorldX, WorldY);
    
    // Basic terrain density: negative below surface, positive above
    // The value represents distance from surface (in voxel units)
    float TerrainDensity = (float)WorldZ - TerrainHeight;
    
    // Add 3D variation for more interesting terrain (overhangs, etc)
    float Variation3D = Get3DTerrainVariation(WorldX, WorldY, WorldZ);
    
    // Reduce 3D variation near surface for more natural terrain
    // and increase it underground for caves and overhangs
    float DepthFromSurface = TerrainHeight - WorldZ;
    float VariationStrength = FMath::Clamp(DepthFromSurface / 20.0f, 0.0f, 1.0f) * 0.5f;
    TerrainDensity += Variation3D * VariationStrength;
    
    // Apply caves
    if (WorldSettings.bGenerateCaves && WorldZ < TerrainHeight - 5 && WorldZ > 2)
    {
        float CaveDensity = GetCaveDensity(WorldX, WorldY, WorldZ);
        
        // If we're in a cave area, use the max of terrain and cave density
        // This carves out caves from solid terrain
        if (CaveDensity > 0.0f)
        {
            TerrainDensity = FMath::Max(TerrainDensity, CaveDensity);
        }
    }
    
    // Bedrock layer - always solid
    if (WorldZ <= 0)
    {
        TerrainDensity = -10.0f; // Very solid
    }
    else if (WorldZ < 3)
    {
        // Transition to bedrock
        float BedrockBlend = (3.0f - WorldZ) / 3.0f;
        TerrainDensity = FMath::Min(TerrainDensity, FMath::Lerp(TerrainDensity, -10.0f, BedrockBlend));
    }
    
    // Water handling - for ocean biomes
    EBiomeType Biome = GetBiome(WorldX, WorldY);
    if (Biome == EBiomeType::Ocean)
    {
        float WaterLevel = WorldSettings.BaseTerrainHeight - 5;
        if (WorldZ <= WaterLevel && TerrainDensity > 0.0f)
        {
            // This area should be water, but we handle that in material, not density
            // Keep density as-is for proper mesh generation
        }
    }
    
    // Normalize density to roughly -1 to 1 range
    // This helps with consistent interpolation
    TerrainDensity = FMath::Clamp(TerrainDensity / 5.0f, -1.0f, 1.0f);
    
    return TerrainDensity;
}

EVoxelType UVoxelTerrainGenerator::GetSurfaceBlock(EBiomeType Biome, int32 WorldX, int32 WorldY, int32 WorldZ, float TerrainHeight) const
{
    float DepthFromSurface = TerrainHeight - WorldZ;
    
    switch (Biome)
    {
    case EBiomeType::Desert:
        if (DepthFromSurface < 4)
            return EVoxelType::Sand;
        break;
        
    case EBiomeType::Tundra:
        if (DepthFromSurface < 1)
            return EVoxelType::Snow;
        if (DepthFromSurface < 3)
            return EVoxelType::Dirt;
        break;
        
    case EBiomeType::Mountains:
        if (TerrainHeight > WorldSettings.BaseTerrainHeight + 20)
        {
            if (DepthFromSurface < 1)
                return EVoxelType::Snow;
        }
        return EVoxelType::Stone;
        
    case EBiomeType::Ocean:
        if (WorldZ < WorldSettings.BaseTerrainHeight - 10)
            return EVoxelType::Sand;
        if (DepthFromSurface < 3)
            return EVoxelType::Sand;
        break;
        
    case EBiomeType::Swamp:
        if (DepthFromSurface < 1)
            return EVoxelType::Grass;
        if (DepthFromSurface < 2)
            return EVoxelType::Clay;
        if (DepthFromSurface < 4)
            return EVoxelType::Dirt;
        break;
        
    case EBiomeType::Forest:
    case EBiomeType::Plains:
    default:
        if (DepthFromSurface < 1)
            return EVoxelType::Grass;
        if (DepthFromSurface < 4)
            return EVoxelType::Dirt;
        break;
    }
    
    return EVoxelType::Stone;
}

EVoxelType UVoxelTerrainGenerator::GetUndergroundBlock(int32 WorldZ, float TerrainHeight, EBiomeType Biome) const
{
    // Bedrock layer
    if (WorldZ <= 0)
        return EVoxelType::Bedrock;
    
    if (WorldZ < 3)
    {
        // Mix bedrock with stone near bottom
        if (NoiseGenerator)
        {
            float BedrockNoise = NoiseGenerator->GetNoise2D(WorldZ * 10.0f, WorldZ * 10.0f);
            if (BedrockNoise > 0.3f * WorldZ)
                return EVoxelType::Bedrock;
        }
    }
    
    // Gravel pockets
    if (NoiseGenerator && WorldZ < TerrainHeight - 10)
    {
        float GravelNoise = NoiseGenerator->GetNoise3D(WorldZ * 0.1f, WorldZ * 0.1f, WorldZ * 0.1f);
        if (GravelNoise > 0.8f)
            return EVoxelType::Gravel;
    }
    
    return EVoxelType::Stone;
}

EVoxelType UVoxelTerrainGenerator::GetVoxelType(int32 WorldX, int32 WorldY, int32 WorldZ) const
{
    float TerrainHeight = GetTerrainHeight(WorldX, WorldY);
    EBiomeType Biome = GetBiome(WorldX, WorldY);
    float Density = GetDensity(WorldX, WorldY, WorldZ);
    
    // If density > 0, it's air (or water)
    if (Density > 0.0f)
    {
        // Check for water in ocean biomes
        float WaterLevel = WorldSettings.BaseTerrainHeight - 5;
        if (Biome == EBiomeType::Ocean && WorldZ <= WaterLevel)
        {
            return EVoxelType::Water;
        }
        return EVoxelType::Air;
    }
    
    // Check for caves
    if (IsCave(WorldX, WorldY, WorldZ))
    {
        return EVoxelType::Air;
    }
    
    // Surface and near-surface blocks
    float DepthFromSurface = TerrainHeight - WorldZ;
    if (DepthFromSurface < 5)
    {
        return GetSurfaceBlock(Biome, WorldX, WorldY, WorldZ, TerrainHeight);
    }
    
    // Underground blocks
    return GetUndergroundBlock(WorldZ, TerrainHeight, Biome);
}
