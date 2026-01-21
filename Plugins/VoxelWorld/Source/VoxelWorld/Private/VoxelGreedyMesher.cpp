// Copyright Your Company. All Rights Reserved.

#include "VoxelGreedyMesher.h"
#include "VoxelChunk.h"

FVoxelGreedyMesher::FVoxelGreedyMesher(int32 InChunkSize, float InVoxelSize)
    : ChunkSize(InChunkSize)
    , VoxelSize(InVoxelSize)
{
    FaceMask.SetNum(ChunkSize * ChunkSize);
}

FColor FVoxelGreedyMesher::GetVoxelColor(EVoxelType Type) const
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

void FVoxelGreedyMesher::AddQuad(
    FVoxelMeshData& OutMeshData,
    const FVector& Position,
    const FVector& DU,
    const FVector& DV,
    int32 Width,
    int32 Height,
    const FVector& Normal,
    EVoxelType VoxelType
)
{
    int32 VertexStart = OutMeshData.Vertices.Num();
    FColor Color = GetVoxelColor(VoxelType);

    // Calculate tangent
    FVector Tangent = DU.GetSafeNormal();

    // Add four vertices for the quad
    FVector Vertices[4] = {
        Position,
        Position + DU * Width,
        Position + DU * Width + DV * Height,
        Position + DV * Height
    };

    FVector2D UVs[4] = {
        FVector2D(0, 0),
        FVector2D(Width, 0),
        FVector2D(Width, Height),
        FVector2D(0, Height)
    };

    for (int32 i = 0; i < 4; ++i)
    {
        OutMeshData.Vertices.Add(Vertices[i]);
        OutMeshData.Normals.Add(Normal);
        OutMeshData.UVs.Add(UVs[i]);
        OutMeshData.VertexColors.Add(Color);
        OutMeshData.Tangents.Add(FProcMeshTangent(Tangent, false));
    }

    // Add triangles (two triangles for the quad)
    OutMeshData.Triangles.Add(VertexStart);
    OutMeshData.Triangles.Add(VertexStart + 1);
    OutMeshData.Triangles.Add(VertexStart + 2);

    OutMeshData.Triangles.Add(VertexStart);
    OutMeshData.Triangles.Add(VertexStart + 2);
    OutMeshData.Triangles.Add(VertexStart + 3);
}

void FVoxelGreedyMesher::GenerateMesh(
    const TArray<FVoxel>& VoxelData,
    TFunction<FVoxel(int32, int32, int32)> GetNeighborVoxel,
    FVoxelMeshData& OutMeshData
)
{
    OutMeshData.Reset();
    OutMeshData.Vertices.Reserve(ChunkSize * ChunkSize * 6);
    OutMeshData.Triangles.Reserve(ChunkSize * ChunkSize * 12);

    // Process each axis (X, Y, Z) and both directions (front and back faces)
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        ProcessSlice(VoxelData, GetNeighborVoxel, OutMeshData, Axis, false);
        ProcessSlice(VoxelData, GetNeighborVoxel, OutMeshData, Axis, true);
    }
}

void FVoxelGreedyMesher::ProcessSlice(
    const TArray<FVoxel>& VoxelData,
    TFunction<FVoxel(int32, int32, int32)> GetNeighborVoxel,
    FVoxelMeshData& OutMeshData,
    int32 Axis,
    bool BackFace
)
{
    // Determine the two axes perpendicular to the main axis
    int32 U = (Axis + 1) % 3;
    int32 V = (Axis + 2) % 3;

    // Direction vectors
    FVector AxisDir = FVector::ZeroVector;
    AxisDir[Axis] = 1.0f;
    
    FVector DU = FVector::ZeroVector;
    DU[U] = VoxelSize;
    
    FVector DV = FVector::ZeroVector;
    DV[V] = VoxelSize;

    // Normal direction
    FVector Normal = BackFace ? -AxisDir : AxisDir;

    // For each slice along the main axis
    for (int32 D = 0; D < ChunkSize; ++D)
    {
        // Reset face mask
        FMemory::Memzero(FaceMask.GetData(), FaceMask.Num() * sizeof(bool));

        // Build mask of visible faces
        TArray<EVoxelType> FaceTypes;
        FaceTypes.SetNum(ChunkSize * ChunkSize);
        FMemory::Memzero(FaceTypes.GetData(), FaceTypes.Num() * sizeof(EVoxelType));

        for (int32 VPos = 0; VPos < ChunkSize; ++VPos)
        {
            for (int32 UPos = 0; UPos < ChunkSize; ++UPos)
            {
                // Calculate 3D coordinates
                int32 Coords[3];
                Coords[Axis] = D;
                Coords[U] = UPos;
                Coords[V] = VPos;

                int32 X = Coords[0];
                int32 Y = Coords[1];
                int32 Z = Coords[2];

                // Get current voxel
                FVoxel CurrentVoxel = VoxelData[GetIndex(X, Y, Z)];

                // Get neighbor voxel
                int32 NeighborCoords[3] = { X, Y, Z };
                NeighborCoords[Axis] += BackFace ? -1 : 1;

                FVoxel NeighborVoxel;
                if (NeighborCoords[Axis] >= 0 && NeighborCoords[Axis] < ChunkSize)
                {
                    NeighborVoxel = VoxelData[GetIndex(NeighborCoords[0], NeighborCoords[1], NeighborCoords[2])];
                }
                else
                {
                    NeighborVoxel = GetNeighborVoxel(NeighborCoords[0], NeighborCoords[1], NeighborCoords[2]);
                }

                // Check if face should be visible
                bool CurrentSolid = CurrentVoxel.IsSolid();
                bool NeighborTransparent = NeighborVoxel.IsTransparent();

                int32 MaskIndex = UPos + VPos * ChunkSize;
                if (CurrentSolid && NeighborTransparent)
                {
                    FaceMask[MaskIndex] = true;
                    FaceTypes[MaskIndex] = CurrentVoxel.Type;
                }
            }
        }

        // Greedy meshing - merge adjacent faces of the same type
        for (int32 VPos = 0; VPos < ChunkSize; ++VPos)
        {
            for (int32 UPos = 0; UPos < ChunkSize;)
            {
                int32 MaskIndex = UPos + VPos * ChunkSize;

                if (!FaceMask[MaskIndex])
                {
                    ++UPos;
                    continue;
                }

                EVoxelType CurrentType = FaceTypes[MaskIndex];

                // Find width of merged quad
                int32 Width = 1;
                while (UPos + Width < ChunkSize)
                {
                    int32 NextIndex = (UPos + Width) + VPos * ChunkSize;
                    if (!FaceMask[NextIndex] || FaceTypes[NextIndex] != CurrentType)
                        break;
                    ++Width;
                }

                // Find height of merged quad
                int32 Height = 1;
                bool Done = false;
                while (VPos + Height < ChunkSize && !Done)
                {
                    for (int32 W = 0; W < Width; ++W)
                    {
                        int32 CheckIndex = (UPos + W) + (VPos + Height) * ChunkSize;
                        if (!FaceMask[CheckIndex] || FaceTypes[CheckIndex] != CurrentType)
                        {
                            Done = true;
                            break;
                        }
                    }
                    if (!Done) ++Height;
                }

                // Calculate quad position
                FVector Position;
                Position[Axis] = (D + (BackFace ? 0 : 1)) * VoxelSize;
                Position[U] = UPos * VoxelSize;
                Position[V] = VPos * VoxelSize;

                // Add the merged quad
                if (BackFace)
                {
                    // Flip winding for back faces
                    AddQuad(OutMeshData, Position + DV * Height, DU, -DV, Width, Height, Normal, CurrentType);
                }
                else
                {
                    AddQuad(OutMeshData, Position, DU, DV, Width, Height, Normal, CurrentType);
                }

                // Mark used faces in mask
                for (int32 H = 0; H < Height; ++H)
                {
                    for (int32 W = 0; W < Width; ++W)
                    {
                        FaceMask[(UPos + W) + (VPos + H) * ChunkSize] = false;
                    }
                }

                UPos += Width;
            }
        }
    }
}
