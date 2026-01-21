#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "VoxelTypes.h"
#include "VoxelMarchingCubes.h"
#include "VoxelChunk.generated.h"

class UVoxelTerrainGenerator;

// Include full definition for TUniquePtr to work correctly

/**
 * Represents a single chunk in the voxel world
 * Supports both blocky and smooth (Marching Cubes) mesh generation
 */
UCLASS()
class VOXELWORLD_API AVoxelChunk : public AActor
{
    GENERATED_BODY()

public:
    AVoxelChunk();

    // Destructor must be defined in .cpp where FVoxelMarchingCubes is complete
    ~AVoxelChunk();

    /** Initialize the chunk with coordinates and settings */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void InitializeChunk(const FChunkCoord& InChunkCoord, const FVoxelWorldSettings& InSettings, UVoxelTerrainGenerator* InGenerator);

    /** Generate voxel data for this chunk */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void GenerateVoxelData();

    /** Build the mesh from voxel data */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void BuildMesh();

    /** Get voxel at local position */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FVoxel GetVoxel(int32 LocalX, int32 LocalY, int32 LocalZ) const;

    /** Set voxel at local position */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void SetVoxel(int32 LocalX, int32 LocalY, int32 LocalZ, const FVoxel& Voxel);

    /** Get density at local position (for smooth terrain) */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    float GetDensity(int32 LocalX, int32 LocalY, int32 LocalZ) const;

    /** Set density at local position (for smooth terrain editing) */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void SetDensity(int32 LocalX, int32 LocalY, int32 LocalZ, float Density);

    /** Modify terrain smoothly - adds/removes material in a sphere */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void ModifyTerrain(const FVector& LocalPosition, float Radius, float Strength, bool bAdd);

    /** Get material type at local position */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    EVoxelType GetMaterial(int32 LocalX, int32 LocalY, int32 LocalZ) const;

    /** Set material type at local position */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void SetMaterial(int32 LocalX, int32 LocalY, int32 LocalZ, EVoxelType Material);

    /** Get chunk coordinate */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FChunkCoord GetChunkCoord() const { return ChunkCoord; }

    /** Check if chunk needs mesh rebuild */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    bool NeedsMeshRebuild() const { return bNeedsMeshRebuild; }

    /** Mark chunk as needing mesh rebuild */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void MarkMeshDirty() { bNeedsMeshRebuild = true; }

    /** Check if chunk is fully generated */
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    bool IsGenerated() const { return bIsGenerated; }

    /** Set neighbor chunks for seamless mesh generation */
    void SetNeighbors(AVoxelChunk* XPos, AVoxelChunk* XNeg, AVoxelChunk* YPos, AVoxelChunk* YNeg, AVoxelChunk* ZPos, AVoxelChunk* ZNeg);

    /** Get density from this chunk or neighbors (used by marching cubes) */
    float GetDensityIncludingNeighbors(int32 LocalX, int32 LocalY, int32 LocalZ) const;

    /** Get material from this chunk or neighbors */
    EVoxelType GetMaterialIncludingNeighbors(int32 LocalX, int32 LocalY, int32 LocalZ) const;

protected:
    virtual void BeginPlay() override;

    /** Procedural mesh component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    /** Chunk coordinate in chunk space */
    UPROPERTY()
    FChunkCoord ChunkCoord;

    /** World settings */
    FVoxelWorldSettings WorldSettings;

    /** Terrain generator reference */
    UPROPERTY()
    TObjectPtr<UVoxelTerrainGenerator> TerrainGenerator;

    /**
     * Density data for smooth terrain (Signed Distance Field)
     * Values < 0 are inside terrain (solid)
     * Values > 0 are outside terrain (air)
     * Size is (ChunkSize+1)^3 to include boundary samples for interpolation
     */
    TArray<float> DensityData;

    /**
     * Material data - stores the material type for each voxel
     * Size is ChunkSize^3
     */
    TArray<EVoxelType> MaterialData;

    /** Marching cubes mesher instance */
    TUniquePtr<FVoxelMarchingCubes> MarchingCubes;

    /** Neighbor chunk references */
    TWeakObjectPtr<AVoxelChunk> NeighborXPos;
    TWeakObjectPtr<AVoxelChunk> NeighborXNeg;
    TWeakObjectPtr<AVoxelChunk> NeighborYPos;
    TWeakObjectPtr<AVoxelChunk> NeighborYNeg;
    TWeakObjectPtr<AVoxelChunk> NeighborZPos;
    TWeakObjectPtr<AVoxelChunk> NeighborZNeg;

    /** State flags */
    bool bIsGenerated = false;
    bool bNeedsMeshRebuild = false;

    /** Convert local coordinates to density array index (ChunkSize+1 grid) */
    FORCEINLINE int32 GetDensityIndex(int32 X, int32 Y, int32 Z) const
    {
        int32 Size = WorldSettings.ChunkSize + 1;
        return X + Y * Size + Z * Size * Size;
    }

    /** Convert local coordinates to material array index (ChunkSize grid) */
    FORCEINLINE int32 GetMaterialIndex(int32 X, int32 Y, int32 Z) const
    {
        return X + Y * WorldSettings.ChunkSize + Z * WorldSettings.ChunkSize * WorldSettings.ChunkSize;
    }

    /** Check if local coordinates are within chunk bounds */
    FORCEINLINE bool IsInBounds(int32 X, int32 Y, int32 Z) const
    {
        return X >= 0 && X < WorldSettings.ChunkSize &&
               Y >= 0 && Y < WorldSettings.ChunkSize &&
               Z >= 0 && Z < WorldSettings.ChunkSize;
    }

    /** Check if coordinates are valid for density grid (includes +1 boundary) */
    FORCEINLINE bool IsInDensityBounds(int32 X, int32 Y, int32 Z) const
    {
        return X >= 0 && X <= WorldSettings.ChunkSize &&
               Y >= 0 && Y <= WorldSettings.ChunkSize &&
               Z >= 0 && Z <= WorldSettings.ChunkSize;
    }

    /** Get color for voxel type */
    FColor GetVoxelColor(EVoxelType Type) const;
};
