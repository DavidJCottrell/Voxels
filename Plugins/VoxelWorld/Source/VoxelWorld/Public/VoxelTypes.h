// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "VoxelTypes.generated.h"

/** LOD levels for voxel chunks */
UENUM(BlueprintType)
enum class EVoxelLOD : uint8
{
    LOD0 = 0    UMETA(DisplayName = "Full Detail"),
    LOD1 = 1    UMETA(DisplayName = "Half Detail"),
    LOD2 = 2    UMETA(DisplayName = "Quarter Detail"),
    LOD3 = 3    UMETA(DisplayName = "Eighth Detail"),
    Culled = 4  UMETA(DisplayName = "Culled/Invisible")
};

/** Chunk state for memory management */
UENUM(BlueprintType)
enum class EChunkState : uint8
{
    Unloaded = 0,
    Loading = 1,
    Generated = 2,
    Meshed = 3,
    PendingUnload = 4
};

/** Mesh data structure for chunk generation */
USTRUCT()
struct VOXELWORLD_API FVoxelMeshData
{
    GENERATED_BODY()

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    void Reset()
    {
        Vertices.Empty();
        Triangles.Empty();
        Normals.Empty();
        UVs.Empty();
        VertexColors.Empty();
        Tangents.Empty();
    }

    void Shrink()
    {
        Vertices.Shrink();
        Triangles.Shrink();
        Normals.Shrink();
        UVs.Shrink();
        VertexColors.Shrink();
        Tangents.Shrink();
    }

    bool IsEmpty() const
    {
        return Vertices.Num() == 0;
    }

    SIZE_T GetAllocatedSize() const
    {
        return Vertices.GetAllocatedSize() +
               Triangles.GetAllocatedSize() +
               Normals.GetAllocatedSize() +
               UVs.GetAllocatedSize() +
               VertexColors.GetAllocatedSize() +
               Tangents.GetAllocatedSize();
    }
};

/** Enumeration for different voxel/block types */
UENUM(BlueprintType)
enum class EVoxelType : uint8
{
    Air = 0         UMETA(DisplayName = "Air"),
    Stone = 1       UMETA(DisplayName = "Stone"),
    Dirt = 2        UMETA(DisplayName = "Dirt"),
    Grass = 3       UMETA(DisplayName = "Grass"),
    Sand = 4        UMETA(DisplayName = "Sand"),
    Water = 5       UMETA(DisplayName = "Water"),
    Snow = 6        UMETA(DisplayName = "Snow"),
    Bedrock = 7     UMETA(DisplayName = "Bedrock"),
    Gravel = 8      UMETA(DisplayName = "Gravel"),
    Clay = 9        UMETA(DisplayName = "Clay"),
    Ice = 10        UMETA(DisplayName = "Ice"),
    Lava = 11       UMETA(DisplayName = "Lava"),
    PlateauStone = 12 UMETA(DisplayName = "Plateau Stone"),
    DarkStone = 13  UMETA(DisplayName = "Dark Stone"),
    RedRock = 14    UMETA(DisplayName = "Red Rock"),

    Max UMETA(Hidden)
};

/** Structure representing a single voxel */
USTRUCT(BlueprintType)
struct VOXELWORLD_API FVoxel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    EVoxelType Type = EVoxelType::Air;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    uint8 Density = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    uint8 LightLevel = 0;

    FVoxel() = default;

    FVoxel(EVoxelType InType, uint8 InDensity = 255)
        : Type(InType), Density(InDensity), LightLevel(0)
    {
    }

    FORCEINLINE bool IsSolid() const
    {
        return Type != EVoxelType::Air && Type != EVoxelType::Water;
    }

    FORCEINLINE bool IsTransparent() const
    {
        return Type == EVoxelType::Air || Type == EVoxelType::Water || Type == EVoxelType::Ice;
    }

    FORCEINLINE bool IsLiquid() const
    {
        return Type == EVoxelType::Water || Type == EVoxelType::Lava;
    }
};

/** Chunk coordinate type */
USTRUCT(BlueprintType)
struct VOXELWORLD_API FChunkCoord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
    int32 X = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
    int32 Y = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
    int32 Z = 0;

    FChunkCoord() = default;
    FChunkCoord(int32 InX, int32 InY, int32 InZ) : X(InX), Y(InY), Z(InZ) {}

    FORCEINLINE bool operator==(const FChunkCoord& Other) const
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z;
    }

    FORCEINLINE bool operator!=(const FChunkCoord& Other) const
    {
        return !(*this == Other);
    }

    friend FORCEINLINE uint32 GetTypeHash(const FChunkCoord& Coord)
    {
        return HashCombine(HashCombine(GetTypeHash(Coord.X), GetTypeHash(Coord.Y)), GetTypeHash(Coord.Z));
    }

    FString ToString() const
    {
        return FString::Printf(TEXT("(%d, %d, %d)"), X, Y, Z);
    }
};

/** Voxel material definition */
USTRUCT(BlueprintType)
struct VOXELWORLD_API FVoxelMaterialDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    EVoxelType VoxelType = EVoxelType::Air;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    FLinearColor BaseColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    float Roughness = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    float Metallic = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    UTexture2D* DiffuseTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    UTexture2D* NormalTexture = nullptr;
};

/** Biome type enumeration - expanded with terrain feature biomes */
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
    Plains = 0      UMETA(DisplayName = "Plains"),
    Desert = 1      UMETA(DisplayName = "Desert"),
    Mountains = 2   UMETA(DisplayName = "Mountains"),
    Forest = 3      UMETA(DisplayName = "Forest"),
    Tundra = 4      UMETA(DisplayName = "Tundra"),
    Ocean = 5       UMETA(DisplayName = "Ocean"),
    Swamp = 6       UMETA(DisplayName = "Swamp"),

    // New terrain feature biomes
    Plateau = 7     UMETA(DisplayName = "Plateau"),
    DeepValley = 8  UMETA(DisplayName = "Deep Valley"),
    Canyon = 9      UMETA(DisplayName = "Canyon"),
    Badlands = 10   UMETA(DisplayName = "Badlands"),
    HighlandPlains = 11 UMETA(DisplayName = "Highland Plains"),

    Max UMETA(Hidden)
};

/** LOD settings for performance tuning - with better defaults for large worlds */
USTRUCT(BlueprintType)
struct VOXELWORLD_API FVoxelLODSettings
{
    GENERATED_BODY()

    /** Distance (in chunks) for LOD0 (full detail) - keep small! */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "1", ClampMax = "10"))
    int32 LOD0Distance = 4;

    /** Distance (in chunks) for LOD1 (half detail) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "2", ClampMax = "20"))
    int32 LOD1Distance = 12;

    /** Distance (in chunks) for LOD2 (quarter detail) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "4", ClampMax = "40"))
    int32 LOD2Distance = 28;

    /** Distance (in chunks) for LOD3 (eighth detail) - beyond this is LOD3 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "8", ClampMax = "64"))
    int32 LOD3Distance = 48;

    /** Distance (in chunks) for collision - chunks beyond this have NO collision */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "1", ClampMax = "10"))
    int32 CollisionDistance = 3;

    /** Get LOD level for a given distance */
    EVoxelLOD GetLODForDistance(float Distance) const
    {
        if (Distance <= LOD0Distance) return EVoxelLOD::LOD0;
        if (Distance <= LOD1Distance) return EVoxelLOD::LOD1;
        if (Distance <= LOD2Distance) return EVoxelLOD::LOD2;
        if (Distance <= LOD3Distance) return EVoxelLOD::LOD3;
        return EVoxelLOD::LOD3; // Still render at lowest LOD beyond LOD3Distance
    }

    /** Get step size for LOD level (for mesh decimation) */
    static int32 GetStepSizeForLOD(EVoxelLOD LOD)
    {
        switch (LOD)
        {
            case EVoxelLOD::LOD0: return 1;
            case EVoxelLOD::LOD1: return 2;
            case EVoxelLOD::LOD2: return 4;
            case EVoxelLOD::LOD3: return 8;
            default: return 8;
        }
    }

    /** Should this chunk have collision? */
    bool ShouldHaveCollision(float Distance) const
    {
        return Distance <= CollisionDistance;
    }
};

/** Biome generation settings */
USTRUCT(BlueprintType)
struct VOXELWORLD_API FBiomeSettings
{
    GENERATED_BODY()

    /** Scale of biome regions (larger = bigger biomes) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.0001", ClampMax = "0.1"))
    float BiomeScale = 0.002f;

    /** Plateau height above base terrain */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "10", ClampMax = "200"))
    int32 PlateauHeight = 60;

    /** How flat the plateau tops are (0 = jagged, 1 = perfectly flat) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PlateauFlatness = 0.85f;

    /** Valley depth below base terrain */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "10", ClampMax = "100"))
    int32 ValleyDepth = 40;

    /** Steepness of plateau/valley walls */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float CliffSteepness = 2.5f;

    /** Blend distance between biomes (in voxels) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "1", ClampMax = "50"))
    int32 BiomeBlendDistance = 20;

    /** Enable varied terrain features */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    bool bEnablePlateaus = true;

    /** Enable deep valleys */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    bool bEnableValleys = true;

    /** Enable canyon features */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    bool bEnableCanyons = true;
};

/** World generation settings - with performance optimizations */
USTRUCT(BlueprintType)
struct VOXELWORLD_API FVoxelWorldSettings
{
    GENERATED_BODY()

    /** Size of each chunk in voxels (must be power of 2) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World", meta = (ClampMin = "8", ClampMax = "64"))
    int32 ChunkSize = 32;

    /** Size of each voxel in world units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World", meta = (ClampMin = "10", ClampMax = "1000"))
    float VoxelSize = 100.0f;

    /** Render distance in chunks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World", meta = (ClampMin = "1", ClampMax = "128"))
    int32 RenderDistance = 64;

    /** Maximum world height in chunks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World", meta = (ClampMin = "1", ClampMax = "24"))
    int32 WorldHeightChunks = 16;

    /** Seed for procedural generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    int32 Seed = 12345;

    /** Base terrain height */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1", ClampMax = "256"))
    int32 BaseTerrainHeight = 96;

    /** Terrain amplitude (height variation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1", ClampMax = "128"))
    int32 TerrainAmplitude = 32;

    /** Noise frequency for terrain generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float NoiseFrequency = 0.01f;

    /** Number of noise octaves for detail */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1", ClampMax = "8"))
    int32 NoiseOctaves = 4;

    /** Enable cave generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    bool bGenerateCaves = true;

    /** Cave threshold (higher = more caves) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CaveThreshold = 0.5f;

    /** Terrain smoothness */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TerrainSmoothness = 0.5f;

    /** Biome generation settings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    FBiomeSettings BiomeSettings;

    // ==========================================
    // Performance Settings
    // ==========================================

    /** LOD settings for distance-based detail reduction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    FVoxelLODSettings LODSettings;

    /** Enable async chunk generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bAsyncGeneration = true;

    /** Enable async collision cooking */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bAsyncCollisionCooking = true;

    /** Number of chunks to generate per frame - increase for faster loading */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "1", ClampMax = "32"))
    int32 ChunksPerFrame = 8;

    /** Number of mesh builds per frame - increase for faster loading */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "1", ClampMax = "16"))
    int32 MeshBuildsPerFrame = 6;

    /** Enable chunk pooling to reduce allocations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bEnableChunkPooling = true;

    /** Maximum number of chunks to keep in pool */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "0", ClampMax = "128"))
    int32 ChunkPoolSize = 64;

    /** Enable vertex deduplication (reduces memory, slight CPU cost) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bDeduplicateVertices = true;

    /** Unload voxel data from chunks beyond this distance (0 = never unload) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "0", ClampMax = "64"))
    int32 DataUnloadDistance = 16;

    /** Skip empty chunks entirely (chunks with no surface) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bSkipEmptyChunks = true;

    /** Prioritize chunks in camera view direction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bPrioritizeViewDirection = true;
};
