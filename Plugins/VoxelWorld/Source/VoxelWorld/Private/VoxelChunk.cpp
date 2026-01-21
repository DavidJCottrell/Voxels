// Copyright Your Company. All Rights Reserved.

#include "VoxelChunk.h"
#include "VoxelTerrainGenerator.h"
#include "VoxelWorldModule.h"

// Face vertices and UVs for a unit cube
namespace VoxelFaceData
{
    // Face indices: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    const FVector FaceNormals[6] = {
        FVector(1, 0, 0),   // +X (Right)
        FVector(-1, 0, 0),  // -X (Left)
        FVector(0, 1, 0),   // +Y (Front)
        FVector(0, -1, 0),  // -Y (Back)
        FVector(0, 0, 1),   // +Z (Top)
        FVector(0, 0, -1)   // -Z (Bottom)
    };

    // Vertex offsets for each face (4 vertices per face)
    const FVector FaceVertices[6][4] = {
        // +X Face
        { FVector(1, 0, 0), FVector(1, 1, 0), FVector(1, 1, 1), FVector(1, 0, 1) },
        // -X Face
        { FVector(0, 1, 0), FVector(0, 0, 0), FVector(0, 0, 1), FVector(0, 1, 1) },
        // +Y Face
        { FVector(1, 1, 0), FVector(0, 1, 0), FVector(0, 1, 1), FVector(1, 1, 1) },
        // -Y Face
        { FVector(0, 0, 0), FVector(1, 0, 0), FVector(1, 0, 1), FVector(0, 0, 1) },
        // +Z Face (Top)
        { FVector(0, 0, 1), FVector(1, 0, 1), FVector(1, 1, 1), FVector(0, 1, 1) },
        // -Z Face (Bottom)
        { FVector(0, 1, 0), FVector(1, 1, 0), FVector(1, 0, 0), FVector(0, 0, 0) }
    };

    const FVector2D FaceUVs[4] = {
        FVector2D(0, 1),
        FVector2D(1, 1),
        FVector2D(1, 0),
        FVector2D(0, 0)
    };

    const int32 FaceTriangles[6] = { 0, 1, 2, 0, 2, 3 };
}

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

void AVoxelChunk::BeginPlay()
{
    Super::BeginPlay();
}

void AVoxelChunk::InitializeChunk(const FChunkCoord& InChunkCoord, const FVoxelWorldSettings& InSettings, UVoxelTerrainGenerator* InGenerator)
{
    ChunkCoord = InChunkCoord;
    WorldSettings = InSettings;
    TerrainGenerator = InGenerator;

    // Pre-allocate voxel data
    int32 TotalVoxels = WorldSettings.ChunkSize * WorldSettings.ChunkSize * WorldSettings.ChunkSize;
    VoxelData.SetNum(TotalVoxels);

    // Set actor position in world space
    FVector WorldPosition(
        ChunkCoord.X * WorldSettings.ChunkSize * WorldSettings.VoxelSize,
        ChunkCoord.Y * WorldSettings.ChunkSize * WorldSettings.VoxelSize,
        ChunkCoord.Z * WorldSettings.ChunkSize * WorldSettings.VoxelSize
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

    TerrainGenerator->GenerateChunkData(ChunkCoord, VoxelData);
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

    int32 Index = GetVoxelIndex(LocalX, LocalY, LocalZ);
    if (Index >= 0 && Index < VoxelData.Num())
    {
        return VoxelData[Index];
    }

    return FVoxel(EVoxelType::Air);
}

void AVoxelChunk::SetVoxel(int32 LocalX, int32 LocalY, int32 LocalZ, const FVoxel& Voxel)
{
    if (!IsInBounds(LocalX, LocalY, LocalZ))
    {
        return;
    }

    int32 Index = GetVoxelIndex(LocalX, LocalY, LocalZ);
    if (Index >= 0 && Index < VoxelData.Num())
    {
        VoxelData[Index] = Voxel;
        bNeedsMeshRebuild = true;
    }
}

FVoxel AVoxelChunk::GetVoxelIncludingNeighbors(int32 LocalX, int32 LocalY, int32 LocalZ) const
{
    // Check if within this chunk's bounds
    if (IsInBounds(LocalX, LocalY, LocalZ))
    {
        return GetVoxel(LocalX, LocalY, LocalZ);
    }

    // Check neighbors
    int32 ChunkSize = WorldSettings.ChunkSize;

    // +X neighbor
    if (LocalX >= ChunkSize && NeighborXPos.IsValid())
    {
        return NeighborXPos->GetVoxel(LocalX - ChunkSize, LocalY, LocalZ);
    }
    // -X neighbor
    if (LocalX < 0 && NeighborXNeg.IsValid())
    {
        return NeighborXNeg->GetVoxel(LocalX + ChunkSize, LocalY, LocalZ);
    }
    // +Y neighbor
    if (LocalY >= ChunkSize && NeighborYPos.IsValid())
    {
        return NeighborYPos->GetVoxel(LocalX, LocalY - ChunkSize, LocalZ);
    }
    // -Y neighbor
    if (LocalY < 0 && NeighborYNeg.IsValid())
    {
        return NeighborYNeg->GetVoxel(LocalX, LocalY + ChunkSize, LocalZ);
    }
    // +Z neighbor
    if (LocalZ >= ChunkSize && NeighborZPos.IsValid())
    {
        return NeighborZPos->GetVoxel(LocalX, LocalY, LocalZ - ChunkSize);
    }
    // -Z neighbor
    if (LocalZ < 0 && NeighborZNeg.IsValid())
    {
        return NeighborZNeg->GetVoxel(LocalX, LocalY, LocalZ + ChunkSize);
    }

    // Default to air if no neighbor
    return FVoxel(EVoxelType::Air);
}

bool AVoxelChunk::ShouldRenderFace(int32 LocalX, int32 LocalY, int32 LocalZ, int32 DirX, int32 DirY, int32 DirZ) const
{
    FVoxel AdjacentVoxel = GetVoxelIncludingNeighbors(LocalX + DirX, LocalY + DirY, LocalZ + DirZ);
    return AdjacentVoxel.IsTransparent();
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

void AVoxelChunk::AddFace(FVoxelMeshData& MeshData, const FVector& Position, int32 FaceIndex, EVoxelType VoxelType) const
{
    int32 VertexStart = MeshData.Vertices.Num();
    FColor VoxelColor = GetVoxelColor(VoxelType);
    float VoxelSize = WorldSettings.VoxelSize;

    // Add vertices
    for (int32 i = 0; i < 4; ++i)
    {
        FVector VertexPos = Position + VoxelFaceData::FaceVertices[FaceIndex][i] * VoxelSize;
        MeshData.Vertices.Add(VertexPos);
        MeshData.Normals.Add(VoxelFaceData::FaceNormals[FaceIndex]);
        MeshData.UVs.Add(VoxelFaceData::FaceUVs[i]);
        MeshData.VertexColors.Add(VoxelColor);
        
        // Calculate tangent (perpendicular to normal in the UV U direction)
        FVector Tangent;
        if (FaceIndex == 4 || FaceIndex == 5) // Top/Bottom faces
        {
            Tangent = FVector(1, 0, 0);
        }
        else
        {
            Tangent = FVector::CrossProduct(VoxelFaceData::FaceNormals[FaceIndex], FVector(0, 0, 1)).GetSafeNormal();
        }
        MeshData.Tangents.Add(FProcMeshTangent(Tangent, false));
    }

    // Add triangles
    for (int32 i = 0; i < 6; ++i)
    {
        MeshData.Triangles.Add(VertexStart + VoxelFaceData::FaceTriangles[i]);
    }
}

void AVoxelChunk::BuildMesh()
{
    if (!bIsGenerated)
    {
        UE_LOG(LogVoxelWorld, Warning, TEXT("Chunk %s: Cannot build mesh - voxels not generated!"), *ChunkCoord.ToString());
        return;
    }

    FVoxelMeshData MeshData;
    MeshData.Vertices.Reserve(WorldSettings.ChunkSize * WorldSettings.ChunkSize * 24); // Rough estimate
    MeshData.Triangles.Reserve(WorldSettings.ChunkSize * WorldSettings.ChunkSize * 36);

    int32 ChunkSize = WorldSettings.ChunkSize;
    float VoxelSize = WorldSettings.VoxelSize;

    // Iterate through all voxels
    for (int32 Z = 0; Z < ChunkSize; ++Z)
    {
        for (int32 Y = 0; Y < ChunkSize; ++Y)
        {
            for (int32 X = 0; X < ChunkSize; ++X)
            {
                FVoxel CurrentVoxel = GetVoxel(X, Y, Z);
                
                // Skip air voxels
                if (!CurrentVoxel.IsSolid())
                {
                    continue;
                }

                FVector VoxelPosition(X * VoxelSize, Y * VoxelSize, Z * VoxelSize);

                // Check each face
                // +X face
                if (ShouldRenderFace(X, Y, Z, 1, 0, 0))
                {
                    AddFace(MeshData, VoxelPosition, 0, CurrentVoxel.Type);
                }
                // -X face
                if (ShouldRenderFace(X, Y, Z, -1, 0, 0))
                {
                    AddFace(MeshData, VoxelPosition, 1, CurrentVoxel.Type);
                }
                // +Y face
                if (ShouldRenderFace(X, Y, Z, 0, 1, 0))
                {
                    AddFace(MeshData, VoxelPosition, 2, CurrentVoxel.Type);
                }
                // -Y face
                if (ShouldRenderFace(X, Y, Z, 0, -1, 0))
                {
                    AddFace(MeshData, VoxelPosition, 3, CurrentVoxel.Type);
                }
                // +Z face (Top)
                if (ShouldRenderFace(X, Y, Z, 0, 0, 1))
                {
                    AddFace(MeshData, VoxelPosition, 4, CurrentVoxel.Type);
                }
                // -Z face (Bottom)
                if (ShouldRenderFace(X, Y, Z, 0, 0, -1))
                {
                    AddFace(MeshData, VoxelPosition, 5, CurrentVoxel.Type);
                }
            }
        }
    }

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
