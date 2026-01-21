// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "VoxelTypes.generated.h"

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

    bool IsEmpty() const
    {
        return Vertices.Num() == 0;
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

    Max UMETA(Hidden)
};

/** Structure representing a single voxel */
USTRUCT(BlueprintType)
struct VOXELWORLD_API FVoxel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    EVoxelType Type = EVoxelType::Air;

    /**
     * Density value for smooth terrain
     * For blocky terrain: 0 = empty, 255 = fully solid
     * For smooth terrain: maps to SDF value where 127 = surface
     */
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

/** Biome type enumeration */
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

    Max UMETA(Hidden)
};

/** World generation settings */
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World", meta = (ClampMin = "1", ClampMax = "32"))
    int32 RenderDistance = 8;

    /** Maximum world height in chunks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World", meta = (ClampMin = "1", ClampMax = "16"))
    int32 WorldHeightChunks = 4;

    /** Seed for procedural generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    int32 Seed = 12345;

    /** Base terrain height */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1", ClampMax = "256"))
    int32 BaseTerrainHeight = 64;

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

    /** Enable async chunk generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bAsyncGeneration = true;

    /** Number of chunks to generate per frame */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "1", ClampMax = "16"))
    int32 ChunksPerFrame = 4;

    /**
     * Terrain smoothness - affects how much 3D noise influences terrain
     * Higher values create more overhangs and varied terrain
     * 0 = minimal overhangs (more like traditional heightmap)
     * 1 = maximum 3D variation (more caves and overhangs)
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TerrainSmoothness = 0.5f;
};
