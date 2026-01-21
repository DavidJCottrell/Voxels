// Copyright Your Company. All Rights Reserved.

#include "VoxelChunk.h"
#include "VoxelTerrainGenerator.h"
#include "VoxelMarchingCubes.h"
#include "VoxelWorldModule.h"

AVoxelChunk::AVoxelChunk()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create mesh component with optimized settings
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->bUseAsyncCooking = true;
    MeshComponent->SetCastShadow(true);

    // Start with collision DISABLED - we'll enable it only for nearby chunks
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
    bCollisionEnabled = false;

    RootComponent = MeshComponent;
}

AVoxelChunk::~AVoxelChunk()
{
    // TUniquePtr auto-cleanup for MarchingCubes
}

void AVoxelChunk::BeginPlay()
{
    Super::BeginPlay();
}

void AVoxelChunk::BeginDestroy()
{
    // Mark as pending kill for async safety
    bPendingKill = true;

    // Clear mesh first to release render resources
    ClearMesh();

    // Clear data arrays
    DensityData.Empty();
    MaterialData.Empty();

    // Clear neighbor references
    NeighborXPos.Reset();
    NeighborXNeg.Reset();
    NeighborYPos.Reset();
    NeighborYNeg.Reset();
    NeighborZPos.Reset();
    NeighborZNeg.Reset();

    Super::BeginDestroy();
}

void AVoxelChunk::InitializeChunk(const FChunkCoord& InChunkCoord, const FVoxelWorldSettings& InSettings, UVoxelTerrainGenerator* InGenerator)
{
    ChunkCoord = InChunkCoord;
    WorldSettings = InSettings;
    TerrainGenerator = InGenerator;
    ChunkState = EChunkState::Loading;

    int32 ChunkSize = WorldSettings.ChunkSize;

    // Pre-allocate density data (ChunkSize+1 for interpolation)
    int32 DensitySize = (ChunkSize + 1) * (ChunkSize + 1) * (ChunkSize + 1);
    DensityData.SetNum(DensitySize);

    // Pre-allocate material data
    int32 MaterialSize = ChunkSize * ChunkSize * ChunkSize;
    MaterialData.SetNum(MaterialSize);

    // Create marching cubes mesher
    MarchingCubes = MakeUnique<FVoxelMarchingCubes>(ChunkSize, WorldSettings.VoxelSize);

    // Set actor position
    FVector WorldPosition(
        ChunkCoord.X * ChunkSize * WorldSettings.VoxelSize,
        ChunkCoord.Y * ChunkSize * WorldSettings.VoxelSize,
        ChunkCoord.Z * ChunkSize * WorldSettings.VoxelSize
    );
    SetActorLocation(WorldPosition);

    bIsGenerated = false;
    bNeedsMeshRebuild = true;
    bHasVoxelData = true;
    bPendingKill = false;
}

void AVoxelChunk::ResetChunk()
{
    // Reset for pooling - keep allocations but clear data
    ChunkState = EChunkState::Unloaded;
    bIsGenerated = false;
    bNeedsMeshRebuild = true;
    bHasVoxelData = false;
    bPendingKill = false;
    CurrentLOD = EVoxelLOD::LOD0;

    // Clear mesh
    ClearMesh();

    // Zero out data but keep allocations
    if (DensityData.Num() > 0)
    {
        FMemory::Memzero(DensityData.GetData(), DensityData.Num() * sizeof(float));
    }
    if (MaterialData.Num() > 0)
    {
        FMemory::Memzero(MaterialData.GetData(), MaterialData.Num() * sizeof(EVoxelType));
    }

    // Clear neighbor references
    NeighborXPos.Reset();
    NeighborXNeg.Reset();
    NeighborYPos.Reset();
    NeighborYNeg.Reset();
    NeighborZPos.Reset();
    NeighborZNeg.Reset();
}

void AVoxelChunk::GenerateVoxelData()
{
    // Check for cancellation
    if (bPendingKill)
    {
        return;
    }

    if (!TerrainGenerator)
    {
        UE_LOG(LogVoxelWorld, Warning, TEXT("Chunk %s: No terrain generator assigned!"), *ChunkCoord.ToString());
        return;
    }

    int32 ChunkSize = WorldSettings.ChunkSize;

    int32 ChunkWorldX = ChunkCoord.X * ChunkSize;
    int32 ChunkWorldY = ChunkCoord.Y * ChunkSize;
    int32 ChunkWorldZ = ChunkCoord.Z * ChunkSize;

    // Generate density data
    for (int32 LocalZ = 0; LocalZ <= ChunkSize; ++LocalZ)
    {
        // Check for cancellation periodically
        if (bPendingKill) return;

        int32 WorldZ = ChunkWorldZ + LocalZ;

        for (int32 LocalY = 0; LocalY <= ChunkSize; ++LocalY)
        {
            int32 WorldY = ChunkWorldY + LocalY;

            for (int32 LocalX = 0; LocalX <= ChunkSize; ++LocalX)
            {
                int32 WorldX = ChunkWorldX + LocalX;

                float Density = TerrainGenerator->GetDensity(WorldX, WorldY, WorldZ);
                int32 Index = GetDensityIndex(LocalX, LocalY, LocalZ);
                DensityData[Index] = Density;
            }
        }
    }

    // Generate material data
    for (int32 LocalZ = 0; LocalZ < ChunkSize; ++LocalZ)
    {
        if (bPendingKill) return;

        int32 WorldZ = ChunkWorldZ + LocalZ;

        for (int32 LocalY = 0; LocalY < ChunkSize; ++LocalY)
        {
            int32 WorldY = ChunkWorldY + LocalY;

            for (int32 LocalX = 0; LocalX < ChunkSize; ++LocalX)
            {
                int32 WorldX = ChunkWorldX + LocalX;

                EVoxelType Material = TerrainGenerator->GetVoxelType(WorldX, WorldY, WorldZ);
                int32 Index = GetMaterialIndex(LocalX, LocalY, LocalZ);
                MaterialData[Index] = Material;
            }
        }
    }

    bIsGenerated = true;
    bHasVoxelData = true;
    bNeedsMeshRebuild = true;
    ChunkState = EChunkState::Generated;

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
    if (!IsInBounds(LocalX, LocalY, LocalZ) || !bHasVoxelData)
    {
        return FVoxel(EVoxelType::Air);
    }

    float Density = GetDensity(LocalX, LocalY, LocalZ);
    EVoxelType Material = GetMaterial(LocalX, LocalY, LocalZ);

    FVoxel Voxel(Material);
    Voxel.Density = static_cast<uint8>(FMath::Clamp((1.0f - Density) * 127.5f + 127.5f, 0.0f, 255.0f));

    return Voxel;
}

void AVoxelChunk::SetVoxel(int32 LocalX, int32 LocalY, int32 LocalZ, const FVoxel& Voxel)
{
    if (!IsInBounds(LocalX, LocalY, LocalZ) || !bHasVoxelData)
    {
        return;
    }

    SetMaterial(LocalX, LocalY, LocalZ, Voxel.Type);

    float Density = (static_cast<float>(Voxel.Density) - 127.5f) / 127.5f;
    Density = -Density;

    SetDensity(LocalX, LocalY, LocalZ, Density);

    bNeedsMeshRebuild = true;
}

float AVoxelChunk::GetDensity(int32 LocalX, int32 LocalY, int32 LocalZ) const
{
    if (!IsInDensityBounds(LocalX, LocalY, LocalZ) || !bHasVoxelData)
    {
        return 1.0f;
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
    if (!IsInDensityBounds(LocalX, LocalY, LocalZ) || !bHasVoxelData)
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
    if (!IsInBounds(LocalX, LocalY, LocalZ) || !bHasVoxelData)
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
    if (!IsInBounds(LocalX, LocalY, LocalZ) || !bHasVoxelData)
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

    if (IsInDensityBounds(LocalX, LocalY, LocalZ))
    {
        return GetDensity(LocalX, LocalY, LocalZ);
    }

    // Check neighbors
    if (LocalX > ChunkSize && NeighborXPos.IsValid())
        return NeighborXPos->GetDensity(LocalX - ChunkSize, LocalY, LocalZ);
    if (LocalX < 0 && NeighborXNeg.IsValid())
        return NeighborXNeg->GetDensity(LocalX + ChunkSize, LocalY, LocalZ);
    if (LocalY > ChunkSize && NeighborYPos.IsValid())
        return NeighborYPos->GetDensity(LocalX, LocalY - ChunkSize, LocalZ);
    if (LocalY < 0 && NeighborYNeg.IsValid())
        return NeighborYNeg->GetDensity(LocalX, LocalY + ChunkSize, LocalZ);
    if (LocalZ > ChunkSize && NeighborZPos.IsValid())
        return NeighborZPos->GetDensity(LocalX, LocalY, LocalZ - ChunkSize);
    if (LocalZ < 0 && NeighborZNeg.IsValid())
        return NeighborZNeg->GetDensity(LocalX, LocalY, LocalZ + ChunkSize);

    // FIX: Instead of returning 1.0f (air), generate the density
    // This prevents holes at chunk boundaries when neighbors aren't loaded
    if (TerrainGenerator)
    {
        int32 WorldX = ChunkCoord.X * ChunkSize + LocalX;
        int32 WorldY = ChunkCoord.Y * ChunkSize + LocalY;
        int32 WorldZ = ChunkCoord.Z * ChunkSize + LocalZ;
        return TerrainGenerator->GetDensity(WorldX, WorldY, WorldZ);
    }

    return 1.0f;  // Fallback only if no terrain generator
}

EVoxelType AVoxelChunk::GetMaterialIncludingNeighbors(int32 LocalX, int32 LocalY, int32 LocalZ) const
{
    int32 ChunkSize = WorldSettings.ChunkSize;

    if (IsInBounds(LocalX, LocalY, LocalZ))
    {
        return GetMaterial(LocalX, LocalY, LocalZ);
    }

    if (LocalX >= ChunkSize && NeighborXPos.IsValid())
        return NeighborXPos->GetMaterial(LocalX - ChunkSize, LocalY, LocalZ);
    if (LocalX < 0 && NeighborXNeg.IsValid())
        return NeighborXNeg->GetMaterial(LocalX + ChunkSize, LocalY, LocalZ);
    if (LocalY >= ChunkSize && NeighborYPos.IsValid())
        return NeighborYPos->GetMaterial(LocalX, LocalY - ChunkSize, LocalZ);
    if (LocalY < 0 && NeighborYNeg.IsValid())
        return NeighborYNeg->GetMaterial(LocalX, LocalY + ChunkSize, LocalZ);
    if (LocalZ >= ChunkSize && NeighborZPos.IsValid())
        return NeighborZPos->GetMaterial(LocalX, LocalY, LocalZ - ChunkSize);
    if (LocalZ < 0 && NeighborZNeg.IsValid())
        return NeighborZNeg->GetMaterial(LocalX, LocalY, LocalZ + ChunkSize);

    return EVoxelType::Air;
}

void AVoxelChunk::ModifyTerrain(const FVector& LocalPosition, float Radius, float Strength, bool bAdd)
{
    if (!bHasVoxelData) return;

    int32 ChunkSize = WorldSettings.ChunkSize;
    float VoxelSize = WorldSettings.VoxelSize;

    int32 VoxelRadius = FMath::CeilToInt(Radius / VoxelSize) + 1;

    int32 CenterX = FMath::FloorToInt(LocalPosition.X / VoxelSize);
    int32 CenterY = FMath::FloorToInt(LocalPosition.Y / VoxelSize);
    int32 CenterZ = FMath::FloorToInt(LocalPosition.Z / VoxelSize);

    for (int32 Z = CenterZ - VoxelRadius; Z <= CenterZ + VoxelRadius; ++Z)
    {
        for (int32 Y = CenterY - VoxelRadius; Y <= CenterY + VoxelRadius; ++Y)
        {
            for (int32 X = CenterX - VoxelRadius; X <= CenterX + VoxelRadius; ++X)
            {
                if (!IsInDensityBounds(X, Y, Z))
                    continue;

                FVector VoxelPos(X * VoxelSize, Y * VoxelSize, Z * VoxelSize);
                float Distance = FVector::Dist(VoxelPos, LocalPosition);

                if (Distance <= Radius)
                {
                    float Falloff = 1.0f - (Distance / Radius);
                    Falloff = FMath::SmoothStep(0.0f, 1.0f, Falloff);

                    float DensityChange = Strength * Falloff;

                    int32 Index = GetDensityIndex(X, Y, Z);
                    if (Index >= 0 && Index < DensityData.Num())
                    {
                        if (bAdd)
                            DensityData[Index] -= DensityChange;
                        else
                            DensityData[Index] += DensityChange;

                        DensityData[Index] = FMath::Clamp(DensityData[Index], -1.0f, 1.0f);
                    }
                }
            }
        }
    }

    bNeedsMeshRebuild = true;
}

void AVoxelChunk::SetLOD(EVoxelLOD NewLOD)
{
    if (CurrentLOD != NewLOD)
    {
        CurrentLOD = NewLOD;
        bNeedsMeshRebuild = true;
    }
}

void AVoxelChunk::SetCollisionEnabled(bool bEnabled)
{
    if (bCollisionEnabled != bEnabled)
    {
        bCollisionEnabled = bEnabled;

        if (MeshComponent)
        {
            if (bEnabled)
            {
                MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
            else
            {
                MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }
    }
}

void AVoxelChunk::UnloadVoxelData()
{
    if (!bHasVoxelData) return;

    // Free memory
    DensityData.Empty();
    MaterialData.Empty();

    bHasVoxelData = false;

    UE_LOG(LogVoxelWorld, Verbose, TEXT("Unloaded voxel data for chunk %s"), *ChunkCoord.ToString());
}

void AVoxelChunk::ReloadVoxelData()
{
    if (bHasVoxelData) return;

    // Re-allocate and regenerate
    int32 ChunkSize = WorldSettings.ChunkSize;
    int32 DensitySize = (ChunkSize + 1) * (ChunkSize + 1) * (ChunkSize + 1);
    int32 MaterialSize = ChunkSize * ChunkSize * ChunkSize;

    DensityData.SetNum(DensitySize);
    MaterialData.SetNum(MaterialSize);

    bHasVoxelData = true;
    bIsGenerated = false;

    // Regenerate data
    GenerateVoxelData();
}

int64 AVoxelChunk::GetMemoryUsage() const
{
    int64 TotalBytes = 0;

    TotalBytes += DensityData.GetAllocatedSize();
    TotalBytes += MaterialData.GetAllocatedSize();

    // Estimate mesh memory (rough approximation)
    if (MeshComponent && MeshComponent->GetProcMeshSection(0))
    {
        auto* Section = MeshComponent->GetProcMeshSection(0);
        TotalBytes += Section->ProcVertexBuffer.GetAllocatedSize();
        TotalBytes += Section->ProcIndexBuffer.GetAllocatedSize();
    }

    return TotalBytes;
}

void AVoxelChunk::CompactMemory()
{
    DensityData.Shrink();
    MaterialData.Shrink();
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

void AVoxelChunk::ClearMesh()
{
    if (MeshComponent)
    {
        MeshComponent->ClearAllMeshSections();
    }
}

void AVoxelChunk::BuildMesh()
{
    BuildMeshWithLOD(CurrentLOD);
}

void AVoxelChunk::BuildMeshWithLOD(EVoxelLOD LODLevel)
{
    if (bPendingKill) return;

    if (!bIsGenerated || !bHasVoxelData)
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

    // Get step size for LOD
    int32 StepSize = FVoxelLODSettings::GetStepSizeForLOD(LODLevel);

    // Create lambda functions for neighbor access
    auto GetNeighborDensity = [this](int32 X, int32 Y, int32 Z) -> float
    {
        return GetDensityIncludingNeighbors(X, Y, Z);
    };

    auto GetNeighborMaterial = [this](int32 X, int32 Y, int32 Z) -> EVoxelType
    {
        return GetMaterialIncludingNeighbors(X, Y, Z);
    };

    // Generate mesh with LOD
    MarchingCubes->GenerateMeshLOD(
        DensityData,
        MaterialData,
        GetNeighborDensity,
        GetNeighborMaterial,
        MeshData,
        StepSize,
        WorldSettings.bDeduplicateVertices
    );

    // Clear existing mesh
    MeshComponent->ClearAllMeshSections();

    // Create mesh if we have data
    if (!MeshData.IsEmpty())
    {
        // Shrink arrays to save memory
        MeshData.Shrink();

        // Create mesh section - collision only if enabled for this chunk
        MeshComponent->CreateMeshSection(
            0,
            MeshData.Vertices,
            MeshData.Triangles,
            MeshData.Normals,
            MeshData.UVs,
            MeshData.VertexColors,
            MeshData.Tangents,
            bCollisionEnabled  // Only create collision for nearby chunks!
        );

        // Set collision state
        if (bCollisionEnabled)
        {
            MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        }
        else
        {
            MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }

    bNeedsMeshRebuild = false;
    ChunkState = EChunkState::Meshed;
    CurrentLOD = LODLevel;

    UE_LOG(LogVoxelWorld, Verbose, TEXT("Built mesh for chunk %s (LOD%d): %d vertices, %d triangles, collision=%s"),
        *ChunkCoord.ToString(),
        static_cast<int32>(LODLevel),
        MeshData.Vertices.Num(),
        MeshData.Triangles.Num() / 3,
        bCollisionEnabled ? TEXT("ON") : TEXT("OFF"));
}
