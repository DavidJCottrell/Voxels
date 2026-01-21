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

    // Use Voronoi-like noise for distinct plateau regions
    float CellX, CellY;
    float VoronoiDist = NoiseGenerator->GetVoronoiNoise2D(
        WorldX * BiomeScale * 0.5f,
        WorldY * BiomeScale * 0.5f,
        CellX, CellY
    );

    // Large-scale plateau regions
    float PlateauNoise = NoiseGenerator->GetFractalNoise2D(
        WorldX * BiomeScale + 10000.0f,
        WorldY * BiomeScale + 10000.0f,
        2, 0.5f, 2.0f
    );

    // Combine: plateaus form where noise is high and we're not at voronoi edges
    float PlateauFactor = PlateauNoise;

    // Create distinct plateau regions
    if (PlateauFactor > 0.55f)
    {
        // Sharp transition to plateau
        float Influence = FMath::SmoothStep(0.55f, 0.65f, PlateauFactor);

        // Reduce influence at edges (voronoi boundaries create natural cliff edges)
        float EdgeFalloff = FMath::Clamp(VoronoiDist * 3.0f, 0.0f, 1.0f);
        Influence *= EdgeFalloff;

        return Influence;
    }

    return 0.0f;
}

float UVoxelTerrainGenerator::GetValleyInfluence(int32 WorldX, int32 WorldY) const
{
    if (!NoiseGenerator || !WorldSettings.BiomeSettings.bEnableValleys) return 0.0f;

    float BiomeScale = WorldSettings.BiomeSettings.BiomeScale;

    // Valley paths using ridged noise (creates natural river-like valleys)
    float ValleyNoise = NoiseGenerator->GetRidgedNoise2D(
        WorldX * BiomeScale * 1.5f + 20000.0f,
        WorldY * BiomeScale * 1.5f + 20000.0f,
        3, 0.5f, 2.0f
    );

    // Secondary noise for valley variation
    float ValleyVar = NoiseGenerator->GetFractalNoise2D(
        WorldX * BiomeScale * 3.0f + 25000.0f,
        WorldY * BiomeScale * 3.0f + 25000.0f,
        2, 0.5f, 2.0f
    );

    // Valleys form in the ridges of ridged noise (inverted)
    float ValleyFactor = 1.0f - ValleyNoise;

    // Only deep valleys where factor is high
    if (ValleyFactor > 0.6f)
    {
        float Influence = FMath::SmoothStep(0.6f, 0.8f, ValleyFactor);

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

    // Canyons: narrow, deep cuts using domain-warped noise
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

    // Warped ridge noise creates winding canyon paths
    float CanyonNoise = NoiseGenerator->GetRidgedNoise2D(
        (WorldX + WarpX) * BiomeScale * 4.0f,
        (WorldY + WarpY) * BiomeScale * 4.0f,
        2, 0.6f, 2.0f
    );

    // Narrow canyon threshold
    if (CanyonNoise > 0.85f)
    {
        return FMath::SmoothStep(0.85f, 0.95f, CanyonNoise);
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
    float BaseHeight = WorldSettings.BaseTerrainHeight;

    // Get feature influences
    float PlateauInf = GetPlateauInfluence(WorldX, WorldY);
    float ValleyInf = GetValleyInfluence(WorldX, WorldY);
    float CanyonInf = GetCanyonInfluence(WorldX, WorldY);

    float FeatureHeight = 0.0f;

    // Plateau: raises terrain with flat top
    if (PlateauInf > 0.0f)
    {
        float PlateauTop = WorldSettings.BiomeSettings.PlateauHeight;
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

    // Add detail noise
    float DetailNoise = NoiseGenerator->GetFractalNoise2D(
        WorldX * WorldSettings.NoiseFrequency * 4.0f,
        WorldY * WorldSettings.NoiseFrequency * 4.0f,
        WorldSettings.NoiseOctaves,
        0.5f, 2.0f
    );
    float DetailVariation = (DetailNoise - 0.5f) * 8.0f;

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
        float PlateauTop = BaseHeight + WorldSettings.BiomeSettings.PlateauHeight;

        // Flatten the top
        float FlattenedHeight = FMath::Lerp(FinalHeight, PlateauTop, Flatness * PlateauInf);

        // Add small variation to plateau tops
        float TopVariation = NoiseGenerator->GetFractalNoise2D(
            WorldX * WorldSettings.NoiseFrequency * 8.0f + 15000.0f,
            WorldY * WorldSettings.NoiseFrequency * 8.0f + 15000.0f,
            2, 0.5f, 2.0f
        );
        FlattenedHeight += (TopVariation - 0.5f) * 3.0f * (1.0f - Flatness);

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
    // FIX 1: Safer 3D variation application
    // =========================================
    // Only apply 3D variation well below the surface to prevent thin spots
    float DepthFromSurface = TerrainHeight - WorldZ;

    if (DepthFromSurface > 3.0f)  // Only apply variation 3+ voxels below surface
    {
        float Variation3D = Get3DTerrainVariation(WorldX, WorldY, WorldZ);

        // Gradual ramp-in of variation (none at depth 3, full at depth 20+)
        float VariationStrength = FMath::Clamp((DepthFromSurface - 3.0f) / 17.0f, 0.0f, 1.0f) * 0.5f;

        // CRITICAL: Only allow variation to make terrain MORE solid, not less
        // This prevents holes from forming near the surface
        if (Variation3D < 0.0f)  // Negative = more solid
        {
            TerrainDensity += Variation3D * VariationStrength;
        }
        else if (DepthFromSurface > 10.0f)  // Only allow "air pockets" deep underground
        {
            TerrainDensity += Variation3D * VariationStrength * 0.5f;  // Reduced strength
        }
    }

    // =========================================
    // FIX 2: Safer plateau cliff handling
    // =========================================
    float PlateauInf = GetPlateauInfluence(WorldX, WorldY);
    if (PlateauInf > 0.1f && PlateauInf < 0.9f)
    {
        float CliffSteepness = WorldSettings.BiomeSettings.CliffSteepness;
        float EdgeFactor = 1.0f - FMath::Abs(PlateauInf - 0.5f) * 2.0f;

        if (EdgeFactor > 0.3f)
        {
            // FIX: Don't multiply - use additive steepening instead
            // This prevents sign flipping and maintains density continuity
            float SteepnessAddition = CliffSteepness * EdgeFactor * FMath::Sign(TerrainDensity);

            // Only steepen if we're clearly solid or clearly air
            if (FMath::Abs(TerrainDensity) > 0.1f)
            {
                TerrainDensity += SteepnessAddition * 0.1f;
            }
        }
    }

    // =========================================
    // FIX 3: Safer cave generation
    // =========================================
    // Increase surface margin and add density threshold check
    const float CaveSurfaceMargin = 10.0f;  // Increased from 5.0f
    const float CaveDensityThreshold = -0.3f;  // Only carve into clearly solid terrain

    if (WorldSettings.bGenerateCaves &&
        WorldZ < TerrainHeight - CaveSurfaceMargin &&
        WorldZ > 2 &&
        TerrainDensity < CaveDensityThreshold)  // NEW: Only carve solid terrain
    {
        float CaveDensity = GetCaveDensity(WorldX, WorldY, WorldZ);

        if (CaveDensity > 0.0f)
        {
            // FIX: Blend caves more smoothly instead of hard max
            // This creates smoother cave walls and prevents sharp transitions
            float CaveBlend = FMath::SmoothStep(0.0f, 0.5f, CaveDensity);
            TerrainDensity = FMath::Lerp(TerrainDensity, CaveDensity, CaveBlend);
        }
    }

    // =========================================
    // Bedrock layer (unchanged)
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

    // Water handling (unchanged)
    EBiomeType Biome = GetBiome(WorldX, WorldY);
    if (Biome == EBiomeType::Ocean || Biome == EBiomeType::DeepValley)
    {
        float WaterLevel = WorldSettings.BaseTerrainHeight - 5;
        if (Biome == EBiomeType::DeepValley)
        {
            float ValleyInf = GetValleyInfluence(WorldX, WorldY);
            WaterLevel = WorldSettings.BaseTerrainHeight - WorldSettings.BiomeSettings.ValleyDepth * ValleyInf + 2;
        }
    }

    // =========================================
    // FIX 4: Prevent paper-thin terrain
    // =========================================
    // If density is very close to zero but should be solid, push it more solid
    const float MinSolidThickness = 0.05f;
    if (TerrainDensity < 0.0f && TerrainDensity > -MinSolidThickness)
    {
        // We have paper-thin terrain - thicken it
        TerrainDensity = -MinSolidThickness;
    }

    // Normalize density
    TerrainDensity = FMath::Clamp(TerrainDensity / 5.0f, -1.0f, 1.0f);

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
