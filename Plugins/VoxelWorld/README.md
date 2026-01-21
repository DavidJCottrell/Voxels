# Voxel World Generator Plugin for Unreal Engine 5.6

A high-performance voxel-based procedural world generation system featuring chunk management, infinite terrain, biome generation, cave systems, and optimized mesh rendering.

## Features

- **Procedural Terrain Generation**: Multi-octave Perlin noise with continental, erosion, and peaks/valleys layers
- **Biome System**: Temperature and moisture-based biome distribution (Plains, Desert, Mountains, Forest, Tundra, Ocean, Swamp)
- **Cave Generation**: 3D noise-based cave systems with depth-based variation
- **Chunk-Based Loading**: Automatic chunk loading/unloading based on player position
- **Optimized Meshing**: Greedy meshing algorithm for reduced triangle count
- **Async Generation**: Multi-threaded chunk generation to prevent frame drops
- **Blueprint Support**: Full Blueprint integration via function library
- **Voxel Editing**: Runtime voxel modification with automatic mesh updates

## Installation

1. Copy the `VoxelWorld` folder to your project's `Plugins` directory
2. Regenerate project files (right-click `.uproject` → Generate Visual Studio project files)
3. Build the project
4. Enable the plugin in Edit → Plugins → Project → Voxel World

## Quick Start

### Basic Setup

1. Create a new level
2. Add a `VoxelWorldManager` actor to the level
3. Configure the world settings in the Details panel
4. Add the `VoxelPlayerTracker` component to your player character/pawn
5. Play!

### World Settings

```cpp
USTRUCT(BlueprintType)
struct FVoxelWorldSettings
{
    int32 ChunkSize = 32;           // Voxels per chunk edge (8-64)
    float VoxelSize = 100.0f;       // World units per voxel
    int32 RenderDistance = 8;       // Chunk render distance
    int32 WorldHeightChunks = 4;    // Vertical chunk count
    int32 Seed = 12345;             // Generation seed
    int32 BaseTerrainHeight = 64;   // Base terrain height
    int32 TerrainAmplitude = 32;    // Height variation
    float NoiseFrequency = 0.01f;   // Noise scale
    int32 NoiseOctaves = 4;         // Detail levels
    bool bGenerateCaves = true;     // Enable caves
    float CaveThreshold = 0.5f;     // Cave density
    bool bAsyncGeneration = true;   // Async chunk gen
    int32 ChunksPerFrame = 4;       // Gen throughput
};
```

### Blueprint Usage

```cpp
// Get voxel at position
FVoxel Voxel = UVoxelBlueprintLibrary::GetVoxelAtPosition(WorldContext, Position);

// Place a voxel
UVoxelBlueprintLibrary::PlaceVoxelAtPosition(WorldContext, Position, EVoxelType::Stone);

// Destroy a voxel
UVoxelBlueprintLibrary::DestroyVoxelAtPosition(WorldContext, Position);

// Raycast for voxel interaction
FVector HitPos, HitNormal;
FVoxel HitVoxel;
bool bHit = UVoxelBlueprintLibrary::VoxelRaycast(WorldContext, Start, End, HitPos, HitNormal, HitVoxel);
```

### C++ Usage

```cpp
// Get the world manager
AVoxelWorldManager* Manager = GetWorld()->SpawnActor<AVoxelWorldManager>();
Manager->WorldSettings.Seed = 42;
Manager->WorldSettings.RenderDistance = 12;
Manager->InitializeWorld();

// Update load center (typically in player tick)
Manager->SetLoadCenter(PlayerLocation);

// Modify voxels
FVoxel NewVoxel(EVoxelType::Stone);
Manager->SetVoxelAtWorldPosition(WorldPosition, NewVoxel);

// Query terrain
float Height = Manager->GetTerrainHeightAtWorldPosition(X, Y);
```

## Architecture

### Core Classes

| Class | Description |
|-------|-------------|
| `AVoxelWorldManager` | Main world controller, handles chunk lifecycle |
| `AVoxelChunk` | Individual chunk actor with mesh generation |
| `UVoxelTerrainGenerator` | Procedural terrain generation logic |
| `UVoxelNoiseGenerator` | Perlin/fractal noise implementation |
| `FVoxelGreedyMesher` | Optimized mesh generation |
| `UVoxelPlayerTracker` | Automatic chunk loading component |
| `UVoxelBlueprintLibrary` | Blueprint function library |

### Voxel Types

```cpp
enum class EVoxelType : uint8
{
    Air,      // Empty space
    Stone,    // Base rock
    Dirt,     // Soil layer
    Grass,    // Surface grass
    Sand,     // Desert/beach
    Water,    // Liquid (transparent)
    Snow,     // Snow cover
    Bedrock,  // Indestructible base
    Gravel,   // Loose stone
    Clay,     // Dense soil
    Ice,      // Frozen water (transparent)
    Lava      // Molten rock
};
```

### Biome Types

```cpp
enum class EBiomeType : uint8
{
    Plains,     // Temperate grassland
    Desert,     // Hot and dry
    Mountains,  // High elevation
    Forest,     // Dense vegetation
    Tundra,     // Cold and frozen
    Ocean,      // Underwater
    Swamp       // Wetlands
};
```

## Performance Optimization

### Greedy Meshing

The plugin includes an optional greedy meshing algorithm that merges adjacent faces of the same type into larger quads, significantly reducing vertex and triangle counts.

To enable greedy meshing, modify `VoxelChunk.cpp`:

```cpp
#include "VoxelGreedyMesher.h"

void AVoxelChunk::BuildMesh()
{
    FVoxelGreedyMesher Mesher(WorldSettings.ChunkSize, WorldSettings.VoxelSize);
    
    auto GetNeighbor = [this](int32 X, int32 Y, int32 Z) -> FVoxel
    {
        return GetVoxelIncludingNeighbors(X, Y, Z);
    };
    
    FVoxelMeshData MeshData;
    Mesher.GenerateMesh(VoxelData, GetNeighbor, MeshData);
    
    // ... apply mesh data
}
```

### Recommended Settings

| Scenario | Chunk Size | Render Distance | Chunks/Frame |
|----------|------------|-----------------|--------------|
| Mobile | 16 | 4 | 2 |
| Desktop | 32 | 8 | 4 |
| High-End | 32 | 16 | 8 |

### Memory Considerations

- Each chunk stores `ChunkSize³` voxels
- Default (32³) = 32,768 voxels per chunk = ~130KB
- Render distance 8 = ~200 chunks loaded = ~26MB voxel data

## Customization

### Adding New Voxel Types

1. Add to `EVoxelType` enum in `VoxelTypes.h`
2. Update `GetVoxelColor()` in `VoxelChunk.cpp`
3. Update `GetVoxelTypeName()` in `VoxelBlueprintLibrary.cpp`
4. Modify terrain generation in `VoxelTerrainGenerator.cpp`

### Custom Terrain Generation

Subclass `UVoxelTerrainGenerator`:

```cpp
UCLASS()
class UMyTerrainGenerator : public UVoxelTerrainGenerator
{
    GENERATED_BODY()
    
public:
    virtual int32 GetTerrainHeight(int32 WorldX, int32 WorldY) const override
    {
        // Custom height function
        return Super::GetTerrainHeight(WorldX, WorldY) + CustomModifier(WorldX, WorldY);
    }
    
    virtual EVoxelType GetVoxelType(int32 WorldX, int32 WorldY, int32 WorldZ) const override
    {
        // Custom voxel placement logic
        return Super::GetVoxelType(WorldX, WorldY, WorldZ);
    }
};
```

### Custom Materials

Assign materials to chunks via the `VoxelWorldManager`:

```cpp
// In Blueprint or C++
VoxelWorldManager->VoxelMaterial = MyCustomMaterial;
```

Vertex colors contain voxel type information for material-based rendering.

## Events and Delegates

The plugin can be extended with delegates for chunk events:

```cpp
// Add to VoxelWorldManager.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkGenerated, FChunkCoord, ChunkCoord);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkDestroyed, FChunkCoord, ChunkCoord);

UPROPERTY(BlueprintAssignable)
FOnChunkGenerated OnChunkGenerated;

UPROPERTY(BlueprintAssignable)
FOnChunkDestroyed OnChunkDestroyed;
```

## Troubleshooting

### Chunks Not Loading
- Ensure `VoxelPlayerTracker` is attached to player
- Check `RenderDistance` is > 0
- Verify `InitializeWorld()` was called

### Poor Performance
- Reduce `ChunksPerFrame`
- Enable `bAsyncGeneration`
- Reduce `RenderDistance`
- Use greedy meshing

### Gaps Between Chunks
- Ensure neighbor chunks are generated before mesh building
- Check `UpdateChunkNeighbors()` is being called

### Terrain Looks Wrong
- Verify `Seed` is consistent
- Check `NoiseFrequency` isn't too high
- Adjust `TerrainAmplitude` and `BaseTerrainHeight`

## License

MIT License - Free for commercial and personal use.

## Version History

- **1.0.0** - Initial release
  - Core voxel system
  - Procedural terrain generation
  - Chunk management
  - Blueprint support
  - Greedy meshing optimization

## Contributing

Contributions are welcome! Please submit pull requests with:
- Clear description of changes
- Test cases if applicable
- Documentation updates

## Support

For issues and feature requests, please use the GitHub issue tracker.
