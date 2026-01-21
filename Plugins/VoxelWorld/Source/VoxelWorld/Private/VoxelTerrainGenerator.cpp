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

    // Clear any cached values
    CachedBiomeNoise.Empty();

    UE_LOG(LogVoxelWorld, Log, TEXT("Terrain generator initialized with seed: %d, Plateaus: %s, Valleys: %s"),
        Settings.Seed,
        Settings.BiomeSettings.bEnablePlateaus ? TEXT("ON") : TEXT("OFF"),
        Settings.BiomeSettings.bEnableValleys ? TEXT("ON") : TEXT("OFF"));
}

// ==========================================
// Biome Feature Queries
// ==========================================

float UVoxelTerrainGenerator::GetPlateauInfluence(int32 WorldX, int32 WorldY) const
{
    if (!NoiseGenerator || !WorldSettings.BiomeSettings.bEnablePlateaus) return 0.0f;

    float BiomeScale = WorldSettings.BiomeSettings.BiomeScale;

    // REMOVED: Voronoi noise was causing the pillar artifacts at cell boundaries
    // Instead, use multiple octaves of fractal noise for smooth, organic edges

    // Primary plateau shape - large scale
    float PlateauNoise1 = NoiseGenerator->GetFractalNoise2D(
        WorldX * BiomeScale + 10000.0f,
        WorldY * BiomeScale + 10000.0f,
        2, 0.5f, 2.0f
    );

    // Secondary noise for edge variation - prevents perfectly round plateaus
    float PlateauNoise2 = NoiseGenerator->GetFractalNoise2D(
        WorldX * BiomeScale * 2.0f + 15000.0f,
        WorldY * BiomeScale * 2.0f + 15000.0f,
        2, 0.5f, 2.0f
    );

    // Combine noises - primary determines plateau presence, secondary adds edge detail
    float CombinedNoise = PlateauNoise1 * 0.7f + PlateauNoise2 * 0.3f;

    // Wide, smooth transition for natural cliff edges
    // Plateaus form where combined noise > 0.45
    float Influence = FMath::SmoothStep(0.45f, 0.65f, CombinedNoise);

    return Influence;
}

float UVoxelTerrainGenerator::GetValleyInfluence(int32 WorldX, int32 WorldY) const
{
    if (!NoiseGenerator || !WorldSettings.BiomeSettings.bEnableValleys) return 0.0f;

    float BiomeScale = WorldSettings.BiomeSettings.BiomeScale;

    // Valley paths using ridged noise
    // CHANGED: Reduced frequency multiplier from 1.5 to 0.8 for WIDER valleys
    float ValleyNoise = NoiseGenerator->GetRidgedNoise2D(
        WorldX * BiomeScale * 0.8f + 20000.0f,  // Was 1.5f - now wider
        WorldY * BiomeScale * 0.8f + 20000.0f,
        3, 0.5f, 2.0f
    );

    // Secondary noise for valley variation
    float ValleyVar = NoiseGenerator->GetFractalNoise2D(
        WorldX * BiomeScale * 2.0f + 25000.0f,
        WorldY * BiomeScale * 2.0f + 25000.0f,
        2, 0.5f, 2.0f
    );

    // Valleys form in the ridges of ridged noise (inverted)
    float ValleyFactor = 1.0f - ValleyNoise;

    // CHANGED: Higher threshold (0.65 instead of 0.6) = fewer, wider valleys
    // Wider smoothstep range (0.65 to 0.85 instead of 0.6 to 0.8) = smoother edges
    if (ValleyFactor > 0.65f)
    {
        float Influence = FMath::SmoothStep(0.65f, 0.85f, ValleyFactor);

        // Vary valley depth
        Influence *= FMath::Lerp(0.6f, 1.0f, ValleyVar);

        return Influence;
    }

    return 0.0f;
}

float UVoxelTerrainGenerator::GetCanyonInfluence(int32 WorldX, int32 WorldY) const
{
    if (!NoiseGenerator || !WorldSettings.BiomeSettings.bEnableCanyons) return 0.0f;

    float BiomeScale = WorldSettings.BiomeSettings.BiomeScale;

    // Domain warping for winding canyon paths
    float WarpX = NoiseGenerator->GetFractalNoise2D(
        WorldX * BiomeScale * 2.0f + 30000.0f,
        WorldY * BiomeScale * 2.0f + 30000.0f,
        2, 0.5f, 2.0f
    ) * 50.0f;

    float WarpY = NoiseGenerator->GetFractalNoise2D(
        WorldX * BiomeScale * 2.0f + 35000.0f,
        WorldY * BiomeScale * 2.0f + 35000.0f,
        2, 0.5f, 2.0f
    ) * 50.0f;

    // CHANGED: Reduced frequency from 4.0 to 2.0 for WIDER canyons
    float CanyonNoise = NoiseGenerator->GetRidgedNoise2D(
        (WorldX + WarpX) * BiomeScale * 2.0f,  // Was 4.0f - now wider
        (WorldY + WarpY) * BiomeScale * 2.0f,
        2, 0.6f, 2.0f
    );

    // CHANGED: Higher threshold (0.88 instead of 0.85) = fewer, wider canyons
    if (CanyonNoise > 0.88f)
    {
        return FMath::SmoothStep(0.88f, 0.95f, CanyonNoise);
    }

    return 0.0f;
}

EBiomeType UVoxelTerrainGenerator::GetTerrainFeature(int32 WorldX, int32 WorldY) const
{
    float PlateauInf = GetPlateauInfluence(WorldX, WorldY);
    float ValleyInf = GetValleyInfluence(WorldX, WorldY);
    float CanyonInf = GetCanyonInfluence(WorldX, WorldY);

    // Priority: Canyon > Valley > Plateau > Base biome
    if (CanyonInf > 0.5f) return EBiomeType::Canyon;
    if (ValleyInf > 0.5f) return EBiomeType::DeepValley;
    if (PlateauInf > 0.5f) return EBiomeType::Plateau;

    return GetBiome(WorldX, WorldY);
}

// ==========================================
// Base Terrain Queries
// ==========================================

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

    // CHANGED: Reduced octaves from 4 to 3, reduced persistence for smoother ridges
    return NoiseGenerator->GetRidgedNoise2D(
        WorldX * Frequency + 2000.0f,
        WorldY * Frequency + 2000.0f,
        3,      // Was 4 - fewer octaves = smoother
        0.4f,   // Was 0.5f - lower persistence = less sharp detail
        2.0f
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

    float Frequency = WorldSettings.NoiseFrequency * 1.5f;
    float Variation = NoiseGenerator->GetFractalNoise3D(
        WorldX * Frequency + 4000.0f,
        WorldY * Frequency + 4000.0f,
        WorldZ * Frequency + 4000.0f,
        3, 0.5f, 2.0f
    );

    return (Variation - 0.5f) * 8.0f;
}

// ==========================================
// Biome Determination
// ==========================================

EBiomeType UVoxelTerrainGenerator::GetBiome(int32 WorldX, int32 WorldY) const
{
    float Temperature = GetTemperature(WorldX, WorldY);
    float Moisture = GetMoisture(WorldX, WorldY);
    float Continentalness = GetContinentalness(WorldX, WorldY);

    // Check for terrain features first
    float PlateauInf = GetPlateauInfluence(WorldX, WorldY);
    float ValleyInf = GetValleyInfluence(WorldX, WorldY);

    if (PlateauInf > 0.6f)
    {
        // Highland plains on top of plateaus
        if (Temperature < 0.3f) return EBiomeType::Tundra;
        return EBiomeType::HighlandPlains;
    }

    if (ValleyInf > 0.6f)
    {
        return EBiomeType::DeepValley;
    }

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
            // Check for badlands in hot, dry areas
            float Erosion = GetErosion(WorldX, WorldY);
            if (Erosion < 0.4f)
            {
                return EBiomeType::Badlands;
            }
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

// ==========================================
// Feature Height Generation
// ==========================================

float UVoxelTerrainGenerator::GetFeatureHeight(int32 WorldX, int32 WorldY) const
{
    // Get feature influences
    float PlateauInf = GetPlateauInfluence(WorldX, WorldY);
    float ValleyInf = GetValleyInfluence(WorldX, WorldY);
    float CanyonInf = GetCanyonInfluence(WorldX, WorldY);

    float FeatureHeight = 0.0f;

    // CHANGED: Plateau height multiplier increased from 1.0x to 2.0x
    // This makes plateaus significantly taller
    if (PlateauInf > 0.0f)
    {
        // Use 2x the configured PlateauHeight for more dramatic plateaus
        float PlateauTop = WorldSettings.BiomeSettings.PlateauHeight * 2.0f;
        FeatureHeight += PlateauInf * PlateauTop;
    }

    // Valley: lowers terrain
    if (ValleyInf > 0.0f)
    {
        float ValleyBottom = -WorldSettings.BiomeSettings.ValleyDepth;
        FeatureHeight += ValleyInf * ValleyBottom;
    }

    // Canyon: deep narrow cuts
    if (CanyonInf > 0.0f)
    {
        float CanyonDepth = -WorldSettings.BiomeSettings.ValleyDepth * 1.5f;
        FeatureHeight += CanyonInf * CanyonDepth;
    }

    return FeatureHeight;
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

    // CHANGED: Reduced detail noise significantly to prevent thin spots
    float DetailNoise = NoiseGenerator->GetFractalNoise2D(
        WorldX * WorldSettings.NoiseFrequency * 4.0f,
        WorldY * WorldSettings.NoiseFrequency * 4.0f,
        WorldSettings.NoiseOctaves,
        0.5f, 2.0f
    );
    float DetailVariation = (DetailNoise - 0.5f) * 2.0f;  // Was 8.0f - reduced to 2.0f

    // Combine base terrain
    float FinalHeight = BaseHeight + ContinentHeight + TerrainVariation + DetailVariation;

    // Add feature height (plateaus, valleys)
    float FeatureHeight = GetFeatureHeight(WorldX, WorldY);
    FinalHeight += FeatureHeight;

    // Apply plateau flatness on top
    float PlateauInf = GetPlateauInfluence(WorldX, WorldY);
    if (PlateauInf > 0.5f)
    {
        float Flatness = WorldSettings.BiomeSettings.PlateauFlatness;
        float PlateauTop = BaseHeight + WorldSettings.BiomeSettings.PlateauHeight * 2.0f;

        // Flatten the top
        float FlattenedHeight = FMath::Lerp(FinalHeight, PlateauTop, Flatness * PlateauInf);

        // CHANGED: Reduced plateau top variation to prevent thin spots
        float TopVariation = NoiseGenerator->GetFractalNoise2D(
            WorldX * WorldSettings.NoiseFrequency * 8.0f + 15000.0f,
            WorldY * WorldSettings.NoiseFrequency * 8.0f + 15000.0f,
            2, 0.5f, 2.0f
        );
        FlattenedHeight += (TopVariation - 0.5f) * 1.0f * (1.0f - Flatness);  // Was 3.0f

        FinalHeight = FlattenedHeight;
    }

    return FinalHeight;
}

// ==========================================
// Density Generation (SDF)
// ==========================================

float UVoxelTerrainGenerator::GetPlateauDensity(int32 WorldX, int32 WorldY, int32 WorldZ, float BaseHeight) const
{
    float PlateauInf = GetPlateauInfluence(WorldX, WorldY);
    if (PlateauInf < 0.01f) return 1.0f; // No plateau here

    float PlateauTop = BaseHeight + WorldSettings.BiomeSettings.PlateauHeight;
    float CliffSteepness = WorldSettings.BiomeSettings.CliffSteepness;

    // Distance from plateau top
    float DistFromTop = WorldZ - PlateauTop;

    if (DistFromTop > 0)
    {
        // Above plateau - air
        return DistFromTop;
    }
    else
    {
        // At or below plateau surface
        // Create steep cliff walls
        float EdgeDist = 1.0f - PlateauInf; // Distance from plateau edge (0 = center, 1 = edge)

        if (EdgeDist > 0.3f)
        {
            // Near cliff edge - steep walls
            float CliffDensity = (WorldZ - (BaseHeight - 5)) * CliffSteepness;
            return FMath::Lerp(-1.0f, CliffDensity, EdgeDist);
        }

        // Inside plateau - solid
        return -1.0f;
    }
}

float UVoxelTerrainGenerator::GetValleyDensity(int32 WorldX, int32 WorldY, int32 WorldZ, float BaseHeight) const
{
    float ValleyInf = GetValleyInfluence(WorldX, WorldY);
    if (ValleyInf < 0.01f) return -1.0f; // No valley here

    float ValleyFloor = BaseHeight - WorldSettings.BiomeSettings.ValleyDepth * ValleyInf;

    // Valley carves into terrain
    if (WorldZ > ValleyFloor)
    {
        // Above valley floor
        float WallSteepness = WorldSettings.BiomeSettings.CliffSteepness * 0.8f;
        float DistFromFloor = WorldZ - ValleyFloor;

        // Smooth valley walls
        return FMath::Min(DistFromFloor * WallSteepness, 1.0f);
    }

    return -1.0f; // Below valley floor - solid
}

float UVoxelTerrainGenerator::GetCanyonDensity(int32 WorldX, int32 WorldY, int32 WorldZ, float BaseHeight) const
{
    float CanyonInf = GetCanyonInfluence(WorldX, WorldY);
    if (CanyonInf < 0.01f) return -1.0f; // No canyon here

    float CanyonFloor = BaseHeight - WorldSettings.BiomeSettings.ValleyDepth * 1.5f * CanyonInf;

    if (WorldZ > CanyonFloor)
    {
        // Very steep canyon walls
        float WallSteepness = WorldSettings.BiomeSettings.CliffSteepness * 1.5f;
        return (WorldZ - CanyonFloor) * WallSteepness;
    }

    return -1.0f;
}

float UVoxelTerrainGenerator::BlendTerrainFeatures(int32 WorldX, int32 WorldY, int32 WorldZ) const
{
    float TerrainHeight = GetTerrainHeight(WorldX, WorldY);
    float BaseDensity = (float)WorldZ - TerrainHeight;

    // Get feature influences
    float PlateauInf = GetPlateauInfluence(WorldX, WorldY);
    float ValleyInf = GetValleyInfluence(WorldX, WorldY);
    float CanyonInf = GetCanyonInfluence(WorldX, WorldY);

    // Blend feature densities
    if (PlateauInf > 0.1f)
    {
        float PlateauDens = GetPlateauDensity(WorldX, WorldY, WorldZ, WorldSettings.BaseTerrainHeight);
        BaseDensity = FMath::Lerp(BaseDensity, PlateauDens, PlateauInf * 0.8f);
    }

    if (ValleyInf > 0.1f)
    {
        float ValleyDens = GetValleyDensity(WorldX, WorldY, WorldZ, TerrainHeight + WorldSettings.BiomeSettings.ValleyDepth * ValleyInf);
        // Valleys carve into terrain - use max to cut away
        if (ValleyDens > 0)
        {
            BaseDensity = FMath::Max(BaseDensity, ValleyDens * ValleyInf);
        }
    }

    if (CanyonInf > 0.1f)
    {
        float CanyonDens = GetCanyonDensity(WorldX, WorldY, WorldZ, TerrainHeight + WorldSettings.BiomeSettings.ValleyDepth * 1.5f * CanyonInf);
        if (CanyonDens > 0)
        {
            BaseDensity = FMath::Max(BaseDensity, CanyonDens * CanyonInf);
        }
    }

    return BaseDensity;
}

float UVoxelTerrainGenerator::GetCaveDensity(int32 WorldX, int32 WorldY, int32 WorldZ) const
{
    if (!WorldSettings.bGenerateCaves || !NoiseGenerator) return -1.0f;

    float TerrainHeight = GetTerrainHeight(WorldX, WorldY);

    // Don't generate caves near surface or at bedrock
    if (WorldZ > TerrainHeight - 5 || WorldZ < 3) return -1.0f;

    // Reduce caves in plateau areas (solid rock)
    float PlateauInf = GetPlateauInfluence(WorldX, WorldY);
    if (PlateauInf > 0.7f) return -1.0f;

    float CaveFrequency = WorldSettings.NoiseFrequency * 3.0f;
    float CaveNoise = NoiseGenerator->GetFractalNoise3D(
        WorldX * CaveFrequency,
        WorldY * CaveFrequency,
        WorldZ * CaveFrequency,
        3, 0.5f, 2.0f
    );

    float CaveNoise2 = NoiseGenerator->GetFractalNoise3D(
        WorldX * CaveFrequency * 0.5f + 3000.0f,
        WorldY * CaveFrequency * 0.5f + 3000.0f,
        WorldZ * CaveFrequency * 0.5f + 3000.0f,
        2, 0.5f, 2.0f
    );

    float CombinedNoise = (CaveNoise + CaveNoise2) * 0.5f;

    float DepthFactor = 1.0f - (float)(WorldZ) / TerrainHeight;
    float AdjustedThreshold = WorldSettings.CaveThreshold - DepthFactor * 0.1f;

    // More caves in valleys
    float ValleyInf = GetValleyInfluence(WorldX, WorldY);
    if (ValleyInf > 0.3f)
    {
        AdjustedThreshold -= 0.1f * ValleyInf;
    }

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

    // Basic terrain density (SDF: negative = solid, positive = air)
    float TerrainDensity = (float)WorldZ - TerrainHeight;

    // =========================================
    // NO 3D variation - it causes holes
    // =========================================

    // =========================================
    // Cave generation (conservative settings)
    // =========================================
    const float CaveSurfaceMargin = 20.0f;  // Very conservative
    const float CaveDensityThreshold = -0.8f;  // Only carve very solid areas

    if (WorldSettings.bGenerateCaves &&
        WorldZ < TerrainHeight - CaveSurfaceMargin &&
        WorldZ > 3 &&
        TerrainDensity < CaveDensityThreshold)
    {
        float CaveDensity = GetCaveDensity(WorldX, WorldY, WorldZ);

        if (CaveDensity > 0.0f)
        {
            float CaveBlend = FMath::SmoothStep(0.0f, 0.5f, CaveDensity);
            TerrainDensity = FMath::Lerp(TerrainDensity, CaveDensity, CaveBlend);
        }
    }

    // =========================================
    // Bedrock layer
    // =========================================
    if (WorldZ <= 0)
    {
        TerrainDensity = -10.0f;
    }
    else if (WorldZ < 3)
    {
        float BedrockBlend = (3.0f - WorldZ) / 3.0f;
        TerrainDensity = FMath::Min(TerrainDensity, FMath::Lerp(TerrainDensity, -10.0f, BedrockBlend));
    }

    // =========================================
    // BULLETPROOF THICKNESS CHECK
    // =========================================
    // Apply a large minimum before normalization
    const float MinSolidThicknessPreNorm = 1.0f;  // Increased from 0.5f
    if (TerrainDensity < 0.0f && TerrainDensity > -MinSolidThicknessPreNorm)
    {
        TerrainDensity = -MinSolidThicknessPreNorm;
    }

    // Normalize density
    TerrainDensity = FMath::Clamp(TerrainDensity / 5.0f, -1.0f, 1.0f);

    // =========================================
    // POST-NORMALIZATION SAFETY CHECK
    // If density is barely positive (0 to 0.1) but we're clearly
    // below where terrain should be, force it solid
    // =========================================
    if (TerrainDensity > -0.15f && TerrainDensity < 0.15f)
    {
        // We're right at the surface boundary - this is where holes happen
        // Check if we SHOULD be solid based on raw height
        float RawDensity = (float)WorldZ - TerrainHeight;
        if (RawDensity < -0.5f)
        {
            // We're clearly below terrain surface, force solid
            TerrainDensity = -0.2f;
        }
    }

    return TerrainDensity;
}


// ==========================================
// Material Selection
// ==========================================

EVoxelType UVoxelTerrainGenerator::GetPlateauMaterial(int32 WorldX, int32 WorldY, int32 WorldZ, float TerrainHeight) const
{
    float DepthFromSurface = TerrainHeight - WorldZ;
    float PlateauInf = GetPlateauInfluence(WorldX, WorldY);

    // Plateau tops
    if (DepthFromSurface < 1 && PlateauInf > 0.7f)
    {
        float Temperature = GetTemperature(WorldX, WorldY);
        if (Temperature < 0.3f) return EVoxelType::Snow;
        return EVoxelType::Grass; // Grassy plateau top
    }

    // Plateau cliff walls
    if (PlateauInf > 0.3f && PlateauInf < 0.8f)
    {
        return EVoxelType::PlateauStone; // Distinctive cliff rock
    }

    // Plateau interior
    if (DepthFromSurface < 5)
    {
        return EVoxelType::Dirt;
    }

    return EVoxelType::Stone;
}

EVoxelType UVoxelTerrainGenerator::GetValleyMaterial(int32 WorldX, int32 WorldY, int32 WorldZ, float TerrainHeight) const
{
    float DepthFromSurface = TerrainHeight - WorldZ;
    float ValleyInf = GetValleyInfluence(WorldX, WorldY);
    float ValleyFloor = WorldSettings.BaseTerrainHeight - WorldSettings.BiomeSettings.ValleyDepth * ValleyInf;

    // Valley floor
    if (WorldZ < ValleyFloor + 3)
    {
        float Moisture = GetMoisture(WorldX, WorldY);
        if (Moisture > 0.6f) return EVoxelType::Clay; // Wet valley floor
        return EVoxelType::Gravel; // Dry riverbed
    }

    // Valley walls
    if (DepthFromSurface < 2)
    {
        return EVoxelType::DarkStone;
    }

    return EVoxelType::Stone;
}

EVoxelType UVoxelTerrainGenerator::GetSurfaceBlock(EBiomeType Biome, int32 WorldX, int32 WorldY, int32 WorldZ, float TerrainHeight) const
{
    float DepthFromSurface = TerrainHeight - WorldZ;

    // Check for special terrain features first
    float PlateauInf = GetPlateauInfluence(WorldX, WorldY);
    float ValleyInf = GetValleyInfluence(WorldX, WorldY);
    float CanyonInf = GetCanyonInfluence(WorldX, WorldY);

    if (PlateauInf > 0.5f)
    {
        return GetPlateauMaterial(WorldX, WorldY, WorldZ, TerrainHeight);
    }

    if (ValleyInf > 0.5f || CanyonInf > 0.5f)
    {
        return GetValleyMaterial(WorldX, WorldY, WorldZ, TerrainHeight);
    }

    switch (Biome)
    {
    case EBiomeType::Desert:
    case EBiomeType::Badlands:
        if (DepthFromSurface < 4)
        {
            if (Biome == EBiomeType::Badlands) return EVoxelType::RedRock;
            return EVoxelType::Sand;
        }
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

    case EBiomeType::HighlandPlains:
        if (DepthFromSurface < 1)
            return EVoxelType::Grass;
        if (DepthFromSurface < 3)
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
    if (WorldZ <= 0)
        return EVoxelType::Bedrock;

    if (WorldZ < 3)
    {
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

    // Deep underground in plateaus/canyons
    if (Biome == EBiomeType::Plateau || Biome == EBiomeType::Canyon)
    {
        return EVoxelType::DarkStone;
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
        // Check for water in valleys and oceans
        float WaterLevel = WorldSettings.BaseTerrainHeight - 5;

        if (Biome == EBiomeType::DeepValley)
        {
            float ValleyInf = GetValleyInfluence(WorldX, WorldY);
            WaterLevel = WorldSettings.BaseTerrainHeight - WorldSettings.BiomeSettings.ValleyDepth * ValleyInf + 5;
        }

        if ((Biome == EBiomeType::Ocean || Biome == EBiomeType::DeepValley) && WorldZ <= WaterLevel)
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

// ==========================================
// Optimization
// ==========================================

bool UVoxelTerrainGenerator::IsChunkLikelyEmpty(int32 ChunkX, int32 ChunkY, int32 ChunkZ, int32 ChunkSize) const
{
    // Quick check corners and center to see if chunk is likely all air or all solid
    int32 WorldBaseX = ChunkX * ChunkSize;
    int32 WorldBaseY = ChunkY * ChunkSize;
    int32 WorldBaseZ = ChunkZ * ChunkSize;

    // Sample points
    const int32 SamplePoints[5][3] = {
        {0, 0, 0},
        {ChunkSize - 1, 0, 0},
        {0, ChunkSize - 1, 0},
        {ChunkSize - 1, ChunkSize - 1, 0},
        {ChunkSize / 2, ChunkSize / 2, ChunkSize / 2}
    };

    int32 AirCount = 0;
    int32 SolidCount = 0;

    for (int32 i = 0; i < 5; ++i)
    {
        float Height = GetTerrainHeight(
            WorldBaseX + SamplePoints[i][0],
            WorldBaseY + SamplePoints[i][1]
        );

        int32 SampleZ = WorldBaseZ + SamplePoints[i][2];

        if (SampleZ > Height + 10) AirCount++;
        else if (SampleZ < Height - 10) SolidCount++;
    }

    // If all samples are air, chunk is likely empty
    return AirCount >= 5;
}
