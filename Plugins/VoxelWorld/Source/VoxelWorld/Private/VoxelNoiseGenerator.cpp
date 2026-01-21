// Copyright Your Company. All Rights Reserved.

#include "VoxelNoiseGenerator.h"

// Gradient vectors for 3D noise
const FVector UVoxelNoiseGenerator::GradientVectors3D[16] = {
    FVector(1, 1, 0), FVector(-1, 1, 0), FVector(1, -1, 0), FVector(-1, -1, 0),
    FVector(1, 0, 1), FVector(-1, 0, 1), FVector(1, 0, -1), FVector(-1, 0, -1),
    FVector(0, 1, 1), FVector(0, -1, 1), FVector(0, 1, -1), FVector(0, -1, -1),
    FVector(1, 1, 0), FVector(-1, 1, 0), FVector(0, -1, 1), FVector(0, -1, -1)
};

UVoxelNoiseGenerator::UVoxelNoiseGenerator()
{
    Permutation.SetNum(512);
    Initialize(12345);
}

void UVoxelNoiseGenerator::Initialize(int32 Seed)
{
    // Create permutation table
    FRandomStream RandomStream(Seed);
    
    TArray<int32> BasePermutation;
    BasePermutation.SetNum(256);
    
    for (int32 i = 0; i < 256; ++i)
    {
        BasePermutation[i] = i;
    }
    
    // Shuffle using Fisher-Yates
    for (int32 i = 255; i > 0; --i)
    {
        int32 j = RandomStream.RandRange(0, i);
        Swap(BasePermutation[i], BasePermutation[j]);
    }
    
    // Duplicate for wrapping
    for (int32 i = 0; i < 256; ++i)
    {
        Permutation[i] = BasePermutation[i];
        Permutation[i + 256] = BasePermutation[i];
    }
}

float UVoxelNoiseGenerator::Gradient2D(int32 Hash, float X, float Y) const
{
    int32 H = Hash & 7;
    float U = H < 4 ? X : Y;
    float V = H < 4 ? Y : X;
    return ((H & 1) ? -U : U) + ((H & 2) ? -2.0f * V : 2.0f * V);
}

float UVoxelNoiseGenerator::Gradient3D(int32 Hash, float X, float Y, float Z) const
{
    const FVector& Grad = GradientVectors3D[Hash & 15];
    return Grad.X * X + Grad.Y * Y + Grad.Z * Z;
}

float UVoxelNoiseGenerator::GetNoise2D(float X, float Y) const
{
    // Find grid cell
    int32 X0 = FMath::FloorToInt(X) & 255;
    int32 Y0 = FMath::FloorToInt(Y) & 255;
    int32 X1 = (X0 + 1) & 255;
    int32 Y1 = (Y0 + 1) & 255;
    
    // Relative position in cell
    float XF = X - FMath::FloorToFloat(X);
    float YF = Y - FMath::FloorToFloat(Y);
    
    // Fade curves
    float U = Fade(XF);
    float V = Fade(YF);
    
    // Hash coordinates
    int32 AA = Permutation[Permutation[X0] + Y0];
    int32 AB = Permutation[Permutation[X0] + Y1];
    int32 BA = Permutation[Permutation[X1] + Y0];
    int32 BB = Permutation[Permutation[X1] + Y1];
    
    // Blend gradients
    float Result = Lerp(
        Lerp(Gradient2D(AA, XF, YF), Gradient2D(BA, XF - 1, YF), U),
        Lerp(Gradient2D(AB, XF, YF - 1), Gradient2D(BB, XF - 1, YF - 1), U),
        V
    );
    
    // Normalize to [0, 1]
    return (Result + 1.0f) * 0.5f;
}

float UVoxelNoiseGenerator::GetNoise3D(float X, float Y, float Z) const
{
    // Find grid cell
    int32 X0 = FMath::FloorToInt(X) & 255;
    int32 Y0 = FMath::FloorToInt(Y) & 255;
    int32 Z0 = FMath::FloorToInt(Z) & 255;
    int32 X1 = (X0 + 1) & 255;
    int32 Y1 = (Y0 + 1) & 255;
    int32 Z1 = (Z0 + 1) & 255;
    
    // Relative position in cell
    float XF = X - FMath::FloorToFloat(X);
    float YF = Y - FMath::FloorToFloat(Y);
    float ZF = Z - FMath::FloorToFloat(Z);
    
    // Fade curves
    float U = Fade(XF);
    float V = Fade(YF);
    float W = Fade(ZF);
    
    // Hash coordinates
    int32 A = Permutation[X0] + Y0;
    int32 AA = Permutation[A] + Z0;
    int32 AB = Permutation[A + 1] + Z0;
    int32 B = Permutation[X1] + Y0;
    int32 BA = Permutation[B] + Z0;
    int32 BB = Permutation[B + 1] + Z0;
    
    // Blend gradients
    float Result = Lerp(
        Lerp(
            Lerp(Gradient3D(Permutation[AA], XF, YF, ZF),
                 Gradient3D(Permutation[BA], XF - 1, YF, ZF), U),
            Lerp(Gradient3D(Permutation[AB], XF, YF - 1, ZF),
                 Gradient3D(Permutation[BB], XF - 1, YF - 1, ZF), U),
            V
        ),
        Lerp(
            Lerp(Gradient3D(Permutation[AA + 1], XF, YF, ZF - 1),
                 Gradient3D(Permutation[BA + 1], XF - 1, YF, ZF - 1), U),
            Lerp(Gradient3D(Permutation[AB + 1], XF, YF - 1, ZF - 1),
                 Gradient3D(Permutation[BB + 1], XF - 1, YF - 1, ZF - 1), U),
            V
        ),
        W
    );
    
    // Normalize to [0, 1]
    return (Result + 1.0f) * 0.5f;
}

float UVoxelNoiseGenerator::GetFractalNoise2D(float X, float Y, int32 Octaves, float Persistence, float Lacunarity) const
{
    float Total = 0.0f;
    float Frequency = 1.0f;
    float Amplitude = 1.0f;
    float MaxValue = 0.0f;
    
    for (int32 i = 0; i < Octaves; ++i)
    {
        Total += GetNoise2D(X * Frequency, Y * Frequency) * Amplitude;
        MaxValue += Amplitude;
        Amplitude *= Persistence;
        Frequency *= Lacunarity;
    }
    
    return Total / MaxValue;
}

float UVoxelNoiseGenerator::GetFractalNoise3D(float X, float Y, float Z, int32 Octaves, float Persistence, float Lacunarity) const
{
    float Total = 0.0f;
    float Frequency = 1.0f;
    float Amplitude = 1.0f;
    float MaxValue = 0.0f;
    
    for (int32 i = 0; i < Octaves; ++i)
    {
        Total += GetNoise3D(X * Frequency, Y * Frequency, Z * Frequency) * Amplitude;
        MaxValue += Amplitude;
        Amplitude *= Persistence;
        Frequency *= Lacunarity;
    }
    
    return Total / MaxValue;
}

float UVoxelNoiseGenerator::GetRidgedNoise2D(float X, float Y, int32 Octaves, float Persistence, float Lacunarity) const
{
    float Total = 0.0f;
    float Frequency = 1.0f;
    float Amplitude = 1.0f;
    float MaxValue = 0.0f;
    
    for (int32 i = 0; i < Octaves; ++i)
    {
        float NoiseValue = GetNoise2D(X * Frequency, Y * Frequency);
        // Convert to ridged
        NoiseValue = 1.0f - FMath::Abs(NoiseValue * 2.0f - 1.0f);
        NoiseValue = NoiseValue * NoiseValue;
        
        Total += NoiseValue * Amplitude;
        MaxValue += Amplitude;
        Amplitude *= Persistence;
        Frequency *= Lacunarity;
    }
    
    return Total / MaxValue;
}

float UVoxelNoiseGenerator::GetBillowNoise2D(float X, float Y, int32 Octaves, float Persistence, float Lacunarity) const
{
    float Total = 0.0f;
    float Frequency = 1.0f;
    float Amplitude = 1.0f;
    float MaxValue = 0.0f;
    
    for (int32 i = 0; i < Octaves; ++i)
    {
        float NoiseValue = GetNoise2D(X * Frequency, Y * Frequency);
        // Convert to billow
        NoiseValue = FMath::Abs(NoiseValue * 2.0f - 1.0f);
        
        Total += NoiseValue * Amplitude;
        MaxValue += Amplitude;
        Amplitude *= Persistence;
        Frequency *= Lacunarity;
    }
    
    return Total / MaxValue;
}

float UVoxelNoiseGenerator::GetVoronoiNoise2D(float X, float Y, float& OutCellX, float& OutCellY) const
{
    int32 CellX = FMath::FloorToInt(X);
    int32 CellY = FMath::FloorToInt(Y);
    
    float MinDist = MAX_FLT;
    OutCellX = 0.0f;
    OutCellY = 0.0f;
    
    // Check 3x3 neighborhood
    for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
    {
        for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
        {
            int32 NeighborX = CellX + OffsetX;
            int32 NeighborY = CellY + OffsetY;
            
            // Get random point in cell using hash
            int32 HashValue = Permutation[(Permutation[NeighborX & 255] + NeighborY) & 255];
            float PointX = NeighborX + (float)(HashValue & 255) / 255.0f;
            
            HashValue = Permutation[(HashValue + 1) & 255];
            float PointY = NeighborY + (float)(HashValue & 255) / 255.0f;
            
            // Calculate distance
            float Dist = FMath::Square(X - PointX) + FMath::Square(Y - PointY);
            
            if (Dist < MinDist)
            {
                MinDist = Dist;
                OutCellX = PointX;
                OutCellY = PointY;
            }
        }
    }
    
    return FMath::Sqrt(MinDist);
}
