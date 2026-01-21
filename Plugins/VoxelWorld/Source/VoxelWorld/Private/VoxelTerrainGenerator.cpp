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

int32 UVoxelTerrainGenerator::GetTerrainHeight(int32 WorldX, int32 WorldY) const
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
    
    // Clamp to valid range
    int32 MaxHeight = WorldSettings.WorldHeightChunks * WorldSettings.ChunkSize - 1;
    return FMath::Clamp(FMath::RoundToInt(FinalHeight), 1, MaxHeight);
}

bool UVoxelTerrainGenerator::IsCave(int32 WorldX, int32 WorldY, int32 WorldZ) const
{
    if (!WorldSettings.bGenerateCaves || !NoiseGenerator) return false;
    
    // Don't generate caves near surface or at bedrock
    int32 TerrainHeight = GetTerrainHeight(WorldX, WorldY);
    if (WorldZ > TerrainHeight - 5 || WorldZ < 3) return false;
    
    // Use 3D noise for cave generation
    float CaveFrequency = WorldSettings.NoiseFrequency * 3.0f;
    float CaveNoise = NoiseGenerator->GetFractalNoise3D(
        WorldX * CaveFrequency,
        WorldY * CaveFrequency,
        WorldZ * CaveFrequency,
        3, 0.5f, 2.0f
    );
    
    // Secondary noise for more varied caves
    float CaveNoise2 = NoiseGenerator->GetFractalNoise3D(
        WorldX * CaveFrequency * 0.5f + 3000.0f,
        WorldY * CaveFrequency * 0.5f + 3000.0f,
        WorldZ * CaveFrequency * 0.5f + 3000.0f,
        2, 0.5f, 2.0f
    );
    
    // Combine noise values
    float CombinedNoise = (CaveNoise + CaveNoise2) * 0.5f;
    
    // Depth-based threshold (more caves deeper underground)
    float DepthFactor = 1.0f - (float)(WorldZ) / (float)(TerrainHeight);
    float AdjustedThreshold = WorldSettings.CaveThreshold - DepthFactor * 0.1f;
    
    return CombinedNoise > AdjustedThreshold;
}

EVoxelType UVoxelTerrainGenerator::GetSurfaceBlock(EBiomeType Biome, int32 WorldX, int32 WorldY, int32 WorldZ, int32 TerrainHeight) const
{
    int32 DepthFromSurface = TerrainHeight - WorldZ;
    
    switch (Biome)
    {
    case EBiomeType::Desert:
        if (DepthFromSurface < 4)
            return EVoxelType::Sand;
        break;
        
    case EBiomeType::Tundra:
        if (DepthFromSurface == 0)
            return EVoxelType::Snow;
        if (DepthFromSurface < 3)
            return EVoxelType::Dirt;
        break;
        
    case EBiomeType::Mountains:
        if (TerrainHeight > WorldSettings.BaseTerrainHeight + 20)
        {
            if (DepthFromSurface == 0)
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
        if (DepthFromSurface == 0)
            return EVoxelType::Grass;
        if (DepthFromSurface < 2)
            return EVoxelType::Clay;
        if (DepthFromSurface < 4)
            return EVoxelType::Dirt;
        break;
        
    case EBiomeType::Forest:
    case EBiomeType::Plains:
    default:
        if (DepthFromSurface == 0)
            return EVoxelType::Grass;
        if (DepthFromSurface < 4)
            return EVoxelType::Dirt;
        break;
    }
    
    return EVoxelType::Stone;
}

EVoxelType UVoxelTerrainGenerator::GetUndergroundBlock(int32 WorldZ, int32 TerrainHeight, EBiomeType Biome) const
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
    int32 TerrainHeight = GetTerrainHeight(WorldX, WorldY);
    EBiomeType Biome = GetBiome(WorldX, WorldY);
    
    // Above terrain
    if (WorldZ > TerrainHeight)
    {
        // Water level for oceans
        int32 WaterLevel = WorldSettings.BaseTerrainHeight - 5;
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
    int32 DepthFromSurface = TerrainHeight - WorldZ;
    if (DepthFromSurface < 5)
    {
        return GetSurfaceBlock(Biome, WorldX, WorldY, WorldZ, TerrainHeight);
    }
    
    // Underground blocks
    return GetUndergroundBlock(WorldZ, TerrainHeight, Biome);
}

void UVoxelTerrainGenerator::GenerateChunkData(const FChunkCoord& ChunkCoord, TArray<FVoxel>& OutVoxelData) const
{
    int32 ChunkSize = WorldSettings.ChunkSize;
    int32 TotalVoxels = ChunkSize * ChunkSize * ChunkSize;
    
    OutVoxelData.SetNum(TotalVoxels);
    
    // Calculate world position of chunk origin
    int32 ChunkWorldX = ChunkCoord.X * ChunkSize;
    int32 ChunkWorldY = ChunkCoord.Y * ChunkSize;
    int32 ChunkWorldZ = ChunkCoord.Z * ChunkSize;
    
    // Generate voxels
    for (int32 LocalZ = 0; LocalZ < ChunkSize; ++LocalZ)
    {
        int32 WorldZ = ChunkWorldZ + LocalZ;
        
        for (int32 LocalY = 0; LocalY < ChunkSize; ++LocalY)
        {
            int32 WorldY = ChunkWorldY + LocalY;
            
            for (int32 LocalX = 0; LocalX < ChunkSize; ++LocalX)
            {
                int32 WorldX = ChunkWorldX + LocalX;
                
                // Calculate array index (flatten 3D to 1D)
                int32 Index = LocalX + LocalY * ChunkSize + LocalZ * ChunkSize * ChunkSize;
                
                // Get voxel type
                EVoxelType VoxelType = GetVoxelType(WorldX, WorldY, WorldZ);
                
                // Create voxel
                OutVoxelData[Index] = FVoxel(VoxelType);
            }
        }
    }
}
