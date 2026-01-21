// Copyright Your Company. All Rights Reserved.

#include "VoxelChunk.h"
#include "VoxelTerrainGenerator.h"
#include "VoxelMarchingCubes.h"
#include "VoxelWorldModule.h"

AVoxelChunk::AVoxelChunk()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create mesh component
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->bUseAsyncCooking = true;
    MeshComponent->SetCastShadow(true);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
    
    RootComponent = MeshComponent;
}

AVoxelChunk::~AVoxelChunk()
{
    // MarchingCubes TUniquePtr will auto-cleanup
}

void AVoxelChunk::BeginPlay()
{
    Super::BeginPlay();
}

void AVoxelChunk::InitializeChunk(const FChunkCoord& InChunkCoord, const FVoxelWorldSettings& InSettings, UVoxelTerrainGenerator* InGenerator)
{
    ChunkCoord = InChunkCoord;
    WorldSettings = InSettings;
    TerrainGenerator = InGenerator;

    int32 ChunkSize = WorldSettings.ChunkSize;

    // Allocate density data (ChunkSize+1 to include boundary for interpolation)
    int32 DensitySize = (ChunkSize + 1) * (ChunkSize + 1) * (ChunkSize + 1);
    DensityData.SetNum(DensitySize);

    // Allocate material data
    int32 MaterialSize = ChunkSize * ChunkSize * ChunkSize;
    MaterialData.SetNum(MaterialSize);

    // Create marching cubes mesher
    MarchingCubes = MakeUnique<FVoxelMarchingCubes>(ChunkSize, WorldSettings.VoxelSize);

    // Set actor position in world space
    FVector WorldPosition(
        ChunkCoord.X * ChunkSize * WorldSettings.VoxelSize,
        ChunkCoord.Y * ChunkSize * WorldSettings.VoxelSize,
        ChunkCoord.Z * ChunkSize * WorldSettings.VoxelSize
    );
    SetActorLocation(WorldPosition);

    bIsGenerated = false;
    bNeedsMeshRebuild = true;
}

void AVoxelChunk::GenerateVoxelData()
{
    if (!TerrainGenerator)
    {
        UE_LOG(LogVoxelWorld, Warning, TEXT("Chunk %s: No terrain generator assigned!"), *ChunkCoord.ToString());
        return;
    }

    int32 ChunkSize = WorldSettings.ChunkSize;

    // Calculate world position of chunk origin
    int32 ChunkWorldX = ChunkCoord.X * ChunkSize;
    int32 ChunkWorldY = ChunkCoord.Y * ChunkSize;
    int32 ChunkWorldZ = ChunkCoord.Z * ChunkSize;

    // Generate density data (SDF values)
    // We need ChunkSize+1 samples per dimension for marching cubes
    for (int32 LocalZ = 0; LocalZ <= ChunkSize; ++LocalZ)
    {
        int32 WorldZ = ChunkWorldZ + LocalZ;

        for (int32 LocalY = 0; LocalY <= ChunkSize; ++LocalY)
        {
            int32 WorldY = ChunkWorldY + LocalY;

            for (int32 LocalX = 0; LocalX <= ChunkSize; ++LocalX)
            {
                int32 WorldX = ChunkWorldX + LocalX;

                // Get density from terrain generator
                float Density = TerrainGenerator->GetDensity(WorldX, WorldY, WorldZ);

                int32 Index = GetDensityIndex(LocalX, LocalY, LocalZ);
                DensityData[Index] = Density;
            }
        }
    }

    // Generate material data
    for (int32 LocalZ = 0; LocalZ < ChunkSize; ++LocalZ)
    {
        int32 WorldZ = ChunkWorldZ + LocalZ;

        for (int32 LocalY = 0; LocalY < ChunkSize; ++LocalY)
        {
            int32 WorldY = ChunkWorldY + LocalY;

            for (int32 LocalX = 0; LocalX < ChunkSize; ++LocalX)
            {
                int32 WorldX = ChunkWorldX + LocalX;

                // Get material type from terrain generator
                EVoxelType Material = TerrainGenerator->GetVoxelType(WorldX, WorldY, WorldZ);

                int32 Index = GetMaterialIndex(LocalX, LocalY, LocalZ);
                MaterialData[Index] = Material;
            }
        }
    }

    bIsGenerated = true;
    bNeedsMeshRebuild = true;

    UE_LOG(LogVoxelWorld, Verbose, TEXT("Generated voxel data for chunk %s"), *ChunkCoord.ToString());
}

void AVoxelChunk::SetNeighbors(AVoxelChunk* XPos, AVoxelChunk* XNeg, AVoxelChunk* YPos, AVoxelChunk* YNeg, AVoxelChunk* ZPos, AVoxelChunk* ZNeg)
{
    NeighborXPos = XPos;
    NeighborXNeg = XNeg;
    NeighborYPos = YPos;
    NeighborYNeg = YNeg;
    NeighborZPos = ZPos;
    NeighborZNeg = ZNeg;
}

FVoxel AVoxelChunk::GetVoxel(int32 LocalX, int32 LocalY, int32 LocalZ) const
{
    if (!IsInBounds(LocalX, LocalY, LocalZ))
    {
        return FVoxel(EVoxelType::Air);
    }

    // For smooth terrain, we derive voxel from density
    float Density = GetDensity(LocalX, LocalY, LocalZ);
    EVoxelType Material = GetMaterial(LocalX, LocalY, LocalZ);

    FVoxel Voxel(Material);
    // Convert density to 0-255 range for compatibility
    Voxel.Density = static_cast<uint8>(FMath::Clamp((1.0f - Density) * 127.5f + 127.5f, 0.0f, 255.0f));

    return Voxel;
}

void AVoxelChunk::SetVoxel(int32 LocalX, int32 LocalY, int32 LocalZ, const FVoxel& Voxel)
{
    if (!IsInBounds(LocalX, LocalY, LocalZ))
    {
        return;
    }

    // Set material
    SetMaterial(LocalX, LocalY, LocalZ, Voxel.Type);

    // Convert density from 0-255 to -1 to 1 range
    float Density = (static_cast<float>(Voxel.Density) - 127.5f) / 127.5f;
    Density = -Density; // Invert so higher values = more solid

    // Set density at this voxel's corner
    SetDensity(LocalX, LocalY, LocalZ, Density);

    bNeedsMeshRebuild = true;
}

float AVoxelChunk::GetDensity(int32 LocalX, int32 LocalY, int32 LocalZ) const
{
    if (!IsInDensityBounds(LocalX, LocalY, LocalZ))
    {
        return 1.0f; // Outside = air
    }

    int32 Index = GetDensityIndex(LocalX, LocalY, LocalZ);
    if (Index >= 0 && Index < DensityData.Num())
    {
        return DensityData[Index];
    }

    return 1.0f;
}

void AVoxelChunk::SetDensity(int32 LocalX, int32 LocalY, int32 LocalZ, float Density)
{
    if (!IsInDensityBounds(LocalX, LocalY, LocalZ))
    {
        return;
    }

    int32 Index = GetDensityIndex(LocalX, LocalY, LocalZ);
    if (Index >= 0 && Index < DensityData.Num())
    {
        DensityData[Index] = Density;
        bNeedsMeshRebuild = true;
    }
}

EVoxelType AVoxelChunk::GetMaterial(int32 LocalX, int32 LocalY, int32 LocalZ) const
{
    if (!IsInBounds(LocalX, LocalY, LocalZ))
    {
        return EVoxelType::Air;
    }

    int32 Index = GetMaterialIndex(LocalX, LocalY, LocalZ);
    if (Index >= 0 && Index < MaterialData.Num())
    {
        return MaterialData[Index];
    }

    return EVoxelType::Air;
}

void AVoxelChunk::SetMaterial(int32 LocalX, int32 LocalY, int32 LocalZ, EVoxelType Material)
{
    if (!IsInBounds(LocalX, LocalY, LocalZ))
    {
        return;
    }

    int32 Index = GetMaterialIndex(LocalX, LocalY, LocalZ);
    if (Index >= 0 && Index < MaterialData.Num())
    {
        MaterialData[Index] = Material;
        bNeedsMeshRebuild = true;
    }
}

float AVoxelChunk::GetDensityIncludingNeighbors(int32 LocalX, int32 LocalY, int32 LocalZ) const
{
    int32 ChunkSize = WorldSettings.ChunkSize;

    // Check if within this chunk's density bounds
    if (IsInDensityBounds(LocalX, LocalY, LocalZ))
    {
        return GetDensity(LocalX, LocalY, LocalZ);
    }

    // Check neighbors
    // +X neighbor
    if (LocalX > ChunkSize && NeighborXPos.IsValid())
    {
        return NeighborXPos->GetDensity(LocalX - ChunkSize, LocalY, LocalZ);
    }
    // -X neighbor
    if (LocalX < 0 && NeighborXNeg.IsValid())
    {
        return NeighborXNeg->GetDensity(LocalX + ChunkSize, LocalY, LocalZ);
    }
    // +Y neighbor
    if (LocalY > ChunkSize && NeighborYPos.IsValid())
    {
        return NeighborYPos->GetDensity(LocalX, LocalY - ChunkSize, LocalZ);
    }
    // -Y neighbor
    if (LocalY < 0 && NeighborYNeg.IsValid())
    {
        return NeighborYNeg->GetDensity(LocalX, LocalY + ChunkSize, LocalZ);
    }
    // +Z neighbor
    if (LocalZ > ChunkSize && NeighborZPos.IsValid())
    {
        return NeighborZPos->GetDensity(LocalX, LocalY, LocalZ - ChunkSize);
    }
    // -Z neighbor
    if (LocalZ < 0 && NeighborZNeg.IsValid())
    {
        return NeighborZNeg->GetDensity(LocalX, LocalY, LocalZ + ChunkSize);
    }

    // Default to air if no neighbor
    return 1.0f;
}

EVoxelType AVoxelChunk::GetMaterialIncludingNeighbors(int32 LocalX, int32 LocalY, int32 LocalZ) const
{
    int32 ChunkSize = WorldSettings.ChunkSize;

    // Check if within this chunk's bounds
    if (IsInBounds(LocalX, LocalY, LocalZ))
    {
        return GetMaterial(LocalX, LocalY, LocalZ);
    }

    // Check neighbors
    if (LocalX >= ChunkSize && NeighborXPos.IsValid())
    {
        return NeighborXPos->GetMaterial(LocalX - ChunkSize, LocalY, LocalZ);
    }
    if (LocalX < 0 && NeighborXNeg.IsValid())
    {
        return NeighborXNeg->GetMaterial(LocalX + ChunkSize, LocalY, LocalZ);
    }
    if (LocalY >= ChunkSize && NeighborYPos.IsValid())
    {
        return NeighborYPos->GetMaterial(LocalX, LocalY - ChunkSize, LocalZ);
    }
    if (LocalY < 0 && NeighborYNeg.IsValid())
    {
        return NeighborYNeg->GetMaterial(LocalX, LocalY + ChunkSize, LocalZ);
    }
    if (LocalZ >= ChunkSize && NeighborZPos.IsValid())
    {
        return NeighborZPos->GetMaterial(LocalX, LocalY, LocalZ - ChunkSize);
    }
    if (LocalZ < 0 && NeighborZNeg.IsValid())
    {
        return NeighborZNeg->GetMaterial(LocalX, LocalY, LocalZ + ChunkSize);
    }

    return EVoxelType::Air;
}

void AVoxelChunk::ModifyTerrain(const FVector& LocalPosition, float Radius, float Strength, bool bAdd)
{
    int32 ChunkSize = WorldSettings.ChunkSize;
    float VoxelSize = WorldSettings.VoxelSize;

    // Convert world-space radius to voxel units
    int32 VoxelRadius = FMath::CeilToInt(Radius / VoxelSize) + 1;

    // Convert local position to voxel coordinates
    int32 CenterX = FMath::FloorToInt(LocalPosition.X / VoxelSize);
    int32 CenterY = FMath::FloorToInt(LocalPosition.Y / VoxelSize);
    int32 CenterZ = FMath::FloorToInt(LocalPosition.Z / VoxelSize);

    // Modify density in sphere
    for (int32 Z = CenterZ - VoxelRadius; Z <= CenterZ + VoxelRadius; ++Z)
    {
        for (int32 Y = CenterY - VoxelRadius; Y <= CenterY + VoxelRadius; ++Y)
        {
            for (int32 X = CenterX - VoxelRadius; X <= CenterX + VoxelRadius; ++X)
            {
                if (!IsInDensityBounds(X, Y, Z))
                {
                    continue;
                }

                // Calculate distance from center
                FVector VoxelPos(X * VoxelSize, Y * VoxelSize, Z * VoxelSize);
                float Distance = FVector::Dist(VoxelPos, LocalPosition);

                if (Distance <= Radius)
                {
                    // Calculate falloff (smooth blend at edges)
                    float Falloff = 1.0f - (Distance / Radius);
                    Falloff = FMath::SmoothStep(0.0f, 1.0f, Falloff);

                    float DensityChange = Strength * Falloff;

                    int32 Index = GetDensityIndex(X, Y, Z);
                    if (Index >= 0 && Index < DensityData.Num())
                    {
                        if (bAdd)
                        {
                            // Adding terrain = decrease density (more solid)
                            DensityData[Index] -= DensityChange;
                        }
                        else
                        {
                            // Removing terrain = increase density (more air)
                            DensityData[Index] += DensityChange;
                        }

                        // Clamp density to reasonable range
                        DensityData[Index] = FMath::Clamp(DensityData[Index], -1.0f, 1.0f);
                    }
                }
            }
        }
    }

    bNeedsMeshRebuild = true;
}

FColor AVoxelChunk::GetVoxelColor(EVoxelType Type) const
{
    switch (Type)
    {
    case EVoxelType::Stone:     return FColor(128, 128, 128);
    case EVoxelType::Dirt:      return FColor(139, 90, 43);
    case EVoxelType::Grass:     return FColor(34, 139, 34);
    case EVoxelType::Sand:      return FColor(238, 214, 175);
    case EVoxelType::Water:     return FColor(64, 164, 223, 180);
    case EVoxelType::Snow:      return FColor(255, 250, 250);
    case EVoxelType::Bedrock:   return FColor(50, 50, 50);
    case EVoxelType::Gravel:    return FColor(160, 160, 160);
    case EVoxelType::Clay:      return FColor(180, 160, 140);
    case EVoxelType::Ice:       return FColor(200, 230, 255, 200);
    case EVoxelType::Lava:      return FColor(255, 100, 0);
    default:                    return FColor::White;
    }
}

void AVoxelChunk::BuildMesh()
{
    if (!bIsGenerated)
    {
        UE_LOG(LogVoxelWorld, Warning, TEXT("Chunk %s: Cannot build mesh - voxels not generated!"), *ChunkCoord.ToString());
        return;
    }

    if (!MarchingCubes)
    {
        UE_LOG(LogVoxelWorld, Warning, TEXT("Chunk %s: Marching cubes mesher not initialized!"), *ChunkCoord.ToString());
        return;
    }

    FVoxelMeshData MeshData;

    // Create lambda functions for neighbor access
    auto GetNeighborDensity = [this](int32 X, int32 Y, int32 Z) -> float
    {
        return GetDensityIncludingNeighbors(X, Y, Z);
    };

    auto GetNeighborMaterial = [this](int32 X, int32 Y, int32 Z) -> EVoxelType
    {
        return GetMaterialIncludingNeighbors(X, Y, Z);
    };

    // Generate mesh using Marching Cubes
    MarchingCubes->GenerateMesh(
        DensityData,
        MaterialData,
        GetNeighborDensity,
        GetNeighborMaterial,
        MeshData
    );

    // Clear existing mesh
    MeshComponent->ClearAllMeshSections();

    // Create mesh if we have data
    if (!MeshData.IsEmpty())
    {
        MeshComponent->CreateMeshSection(
            0,
            MeshData.Vertices,
            MeshData.Triangles,
            MeshData.Normals,
            MeshData.UVs,
            MeshData.VertexColors,
            MeshData.Tangents,
            true // Create collision
        );

        // Enable collision
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }

    bNeedsMeshRebuild = false;

    UE_LOG(LogVoxelWorld, Verbose, TEXT("Built mesh for chunk %s: %d vertices, %d triangles"),
        *ChunkCoord.ToString(), MeshData.Vertices.Num(), MeshData.Triangles.Num() / 3);
}
