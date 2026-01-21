// Copyright Your Company. All Rights Reserved.

#include "VoxelWorldManager.h"
#include "VoxelChunk.h"
#include "VoxelTerrainGenerator.h"
#include "VoxelWorldModule.h"
#include "Async/Async.h"
#include "Engine/World.h"

void FChunkGenerationTask::DoWork()
{
    if (Chunk && IsValid(Chunk))
    {
        Chunk->GenerateVoxelData();
    }
}

AVoxelWorldManager::AVoxelWorldManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.0f; // Tick every frame
}

void AVoxelWorldManager::BeginPlay()
{
    Super::BeginPlay();

    // Auto-initialize if not done manually
    if (!bIsInitialized)
    {
        InitializeWorld();
    }
}

void AVoxelWorldManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clean up all chunks
    for (auto& Pair : LoadedChunks)
    {
        if (Pair.Value && IsValid(Pair.Value))
        {
            Pair.Value->Destroy();
        }
    }
    LoadedChunks.Empty();
    ChunkGenerationQueue.Empty();
    MeshBuildQueue.Empty();

    Super::EndPlay(EndPlayReason);
}

void AVoxelWorldManager::InitializeWorld()
{
    if (bIsInitialized)
    {
        UE_LOG(LogVoxelWorld, Warning, TEXT("World already initialized!"));
        return;
    }

    // Create terrain generator
    TerrainGenerator = NewObject<UVoxelTerrainGenerator>(this);
    TerrainGenerator->Initialize(WorldSettings);

    CurrentLoadCenter = FChunkCoord(0, 0, 0);
    bIsInitialized = true;

    UE_LOG(LogVoxelWorld, Log, TEXT("Voxel World initialized with seed %d, chunk size %d, render distance %d"),
        WorldSettings.Seed, WorldSettings.ChunkSize, WorldSettings.RenderDistance);

    // Start loading chunks around origin
    UpdateChunkLoading();
}

void AVoxelWorldManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsInitialized) return;

    // Process queues
    ProcessGenerationQueue();
    ProcessMeshBuildQueue();
}

void AVoxelWorldManager::SetLoadCenter(const FVector& WorldPosition)
{
    FChunkCoord NewCenter = WorldToChunkCoord(WorldPosition);

    if (NewCenter != CurrentLoadCenter)
    {
        CurrentLoadCenter = NewCenter;
        UpdateChunkLoading();
    }
}

FChunkCoord AVoxelWorldManager::WorldToChunkCoord(const FVector& WorldPosition) const
{
    float ChunkWorldSize = WorldSettings.ChunkSize * WorldSettings.VoxelSize;

    return FChunkCoord(
        FMath::FloorToInt(WorldPosition.X / ChunkWorldSize),
        FMath::FloorToInt(WorldPosition.Y / ChunkWorldSize),
        FMath::FloorToInt(WorldPosition.Z / ChunkWorldSize)
    );
}

void AVoxelWorldManager::WorldToLocalVoxelCoord(const FVector& WorldPosition, FChunkCoord& OutChunkCoord, int32& OutLocalX, int32& OutLocalY, int32& OutLocalZ) const
{
    OutChunkCoord = WorldToChunkCoord(WorldPosition);

    float ChunkWorldSize = WorldSettings.ChunkSize * WorldSettings.VoxelSize;

    // Get position relative to chunk origin
    FVector RelativePos = WorldPosition - FVector(
        OutChunkCoord.X * ChunkWorldSize,
        OutChunkCoord.Y * ChunkWorldSize,
        OutChunkCoord.Z * ChunkWorldSize
    );

    // Convert to voxel coordinates
    OutLocalX = FMath::FloorToInt(RelativePos.X / WorldSettings.VoxelSize);
    OutLocalY = FMath::FloorToInt(RelativePos.Y / WorldSettings.VoxelSize);
    OutLocalZ = FMath::FloorToInt(RelativePos.Z / WorldSettings.VoxelSize);

    // Clamp to valid range
    OutLocalX = FMath::Clamp(OutLocalX, 0, WorldSettings.ChunkSize - 1);
    OutLocalY = FMath::Clamp(OutLocalY, 0, WorldSettings.ChunkSize - 1);
    OutLocalZ = FMath::Clamp(OutLocalZ, 0, WorldSettings.ChunkSize - 1);
}

AVoxelChunk* AVoxelWorldManager::GetChunk(const FChunkCoord& ChunkCoord) const
{
    const TObjectPtr<AVoxelChunk>* ChunkPtr = LoadedChunks.Find(ChunkCoord);
    return ChunkPtr ? *ChunkPtr : nullptr;
}

FVoxel AVoxelWorldManager::GetVoxelAtWorldPosition(const FVector& WorldPosition) const
{
    FChunkCoord ChunkCoord;
    int32 LocalX, LocalY, LocalZ;
    WorldToLocalVoxelCoord(WorldPosition, ChunkCoord, LocalX, LocalY, LocalZ);

    AVoxelChunk* Chunk = GetChunk(ChunkCoord);
    if (Chunk && Chunk->IsGenerated())
    {
        return Chunk->GetVoxel(LocalX, LocalY, LocalZ);
    }

    return FVoxel(EVoxelType::Air);
}

void AVoxelWorldManager::SetVoxelAtWorldPosition(const FVector& WorldPosition, const FVoxel& Voxel)
{
    FChunkCoord ChunkCoord;
    int32 LocalX, LocalY, LocalZ;
    WorldToLocalVoxelCoord(WorldPosition, ChunkCoord, LocalX, LocalY, LocalZ);

    AVoxelChunk* Chunk = GetChunk(ChunkCoord);
    if (Chunk && Chunk->IsGenerated())
    {
        Chunk->SetVoxel(LocalX, LocalY, LocalZ, Voxel);
        
        // Add to mesh rebuild queue if not already there
        if (!MeshBuildQueue.Contains(Chunk))
        {
            MeshBuildQueue.Add(Chunk);
        }

        // Also update adjacent chunks if voxel is on boundary
        if (LocalX == 0)
        {
            AVoxelChunk* Neighbor = GetChunk(FChunkCoord(ChunkCoord.X - 1, ChunkCoord.Y, ChunkCoord.Z));
            if (Neighbor && !MeshBuildQueue.Contains(Neighbor))
                MeshBuildQueue.Add(Neighbor);
        }
        else if (LocalX == WorldSettings.ChunkSize - 1)
        {
            AVoxelChunk* Neighbor = GetChunk(FChunkCoord(ChunkCoord.X + 1, ChunkCoord.Y, ChunkCoord.Z));
            if (Neighbor && !MeshBuildQueue.Contains(Neighbor))
                MeshBuildQueue.Add(Neighbor);
        }

        if (LocalY == 0)
        {
            AVoxelChunk* Neighbor = GetChunk(FChunkCoord(ChunkCoord.X, ChunkCoord.Y - 1, ChunkCoord.Z));
            if (Neighbor && !MeshBuildQueue.Contains(Neighbor))
                MeshBuildQueue.Add(Neighbor);
        }
        else if (LocalY == WorldSettings.ChunkSize - 1)
        {
            AVoxelChunk* Neighbor = GetChunk(FChunkCoord(ChunkCoord.X, ChunkCoord.Y + 1, ChunkCoord.Z));
            if (Neighbor && !MeshBuildQueue.Contains(Neighbor))
                MeshBuildQueue.Add(Neighbor);
        }

        if (LocalZ == 0)
        {
            AVoxelChunk* Neighbor = GetChunk(FChunkCoord(ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z - 1));
            if (Neighbor && !MeshBuildQueue.Contains(Neighbor))
                MeshBuildQueue.Add(Neighbor);
        }
        else if (LocalZ == WorldSettings.ChunkSize - 1)
        {
            AVoxelChunk* Neighbor = GetChunk(FChunkCoord(ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z + 1));
            if (Neighbor && !MeshBuildQueue.Contains(Neighbor))
                MeshBuildQueue.Add(Neighbor);
        }
    }
}

float AVoxelWorldManager::GetTerrainHeightAtWorldPosition(float WorldX, float WorldY) const
{
    if (!TerrainGenerator) return 0.0f;

    // Convert to voxel coordinates
    int32 VoxelX = FMath::FloorToInt(WorldX / WorldSettings.VoxelSize);
    int32 VoxelY = FMath::FloorToInt(WorldY / WorldSettings.VoxelSize);

    int32 TerrainHeight = TerrainGenerator->GetTerrainHeight(VoxelX, VoxelY);
    return TerrainHeight * WorldSettings.VoxelSize;
}

void AVoxelWorldManager::UpdateChunkLoading()
{
    int32 RenderDist = WorldSettings.RenderDistance;
    int32 HeightChunks = WorldSettings.WorldHeightChunks;

    // Determine which chunks should be loaded
    TSet<FChunkCoord> ChunksToKeep;

    for (int32 X = CurrentLoadCenter.X - RenderDist; X <= CurrentLoadCenter.X + RenderDist; ++X)
    {
        for (int32 Y = CurrentLoadCenter.Y - RenderDist; Y <= CurrentLoadCenter.Y + RenderDist; ++Y)
        {
            for (int32 Z = 0; Z < HeightChunks; ++Z)
            {
                FChunkCoord Coord(X, Y, Z);
                float Distance = GetChunkDistanceFromCenter(Coord);
                
                if (Distance <= RenderDist)
                {
                    ChunksToKeep.Add(Coord);

                    // Queue for loading if not already loaded
                    if (!LoadedChunks.Contains(Coord) && !ChunkGenerationQueue.Contains(Coord))
                    {
                        ChunkGenerationQueue.Add(Coord);
                    }
                }
            }
        }
    }

    // Sort generation queue by distance
    SortQueueByDistance(ChunkGenerationQueue);

    // Unload chunks that are too far away
    TArray<FChunkCoord> ChunksToUnload;
    for (auto& Pair : LoadedChunks)
    {
        if (!ChunksToKeep.Contains(Pair.Key))
        {
            ChunksToUnload.Add(Pair.Key);
        }
    }

    for (const FChunkCoord& Coord : ChunksToUnload)
    {
        DestroyChunk(Coord);
    }
}

AVoxelChunk* AVoxelWorldManager::CreateChunk(const FChunkCoord& ChunkCoord)
{
    if (LoadedChunks.Contains(ChunkCoord))
    {
        return LoadedChunks[ChunkCoord];
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;

    AVoxelChunk* NewChunk = GetWorld()->SpawnActor<AVoxelChunk>(AVoxelChunk::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    
    if (NewChunk)
    {
        NewChunk->InitializeChunk(ChunkCoord, WorldSettings, TerrainGenerator);
        
        // Apply material if set
        if (VoxelMaterial)
        {
            UProceduralMeshComponent* MeshComp = NewChunk->FindComponentByClass<UProceduralMeshComponent>();
            if (MeshComp)
            {
                MeshComp->SetMaterial(0, VoxelMaterial);
            }
        }

        LoadedChunks.Add(ChunkCoord, NewChunk);
        UpdateChunkNeighbors(NewChunk);
    }

    return NewChunk;
}

void AVoxelWorldManager::DestroyChunk(const FChunkCoord& ChunkCoord)
{
    TObjectPtr<AVoxelChunk>* ChunkPtr = LoadedChunks.Find(ChunkCoord);
    if (ChunkPtr && *ChunkPtr)
    {
        // Remove from mesh build queue
        MeshBuildQueue.Remove(*ChunkPtr);
        
        (*ChunkPtr)->Destroy();
        LoadedChunks.Remove(ChunkCoord);
    }
}

void AVoxelWorldManager::ProcessGenerationQueue()
{
    int32 ChunksProcessed = 0;

    while (ChunkGenerationQueue.Num() > 0 && ChunksProcessed < WorldSettings.ChunksPerFrame)
    {
        FChunkCoord Coord = ChunkGenerationQueue[0];
        ChunkGenerationQueue.RemoveAt(0);

        // Create chunk if not exists
        AVoxelChunk* Chunk = CreateChunk(Coord);
        
        if (Chunk && !Chunk->IsGenerated())
        {
            if (WorldSettings.bAsyncGeneration)
            {
                // Async generation
                (new FAutoDeleteAsyncTask<FChunkGenerationTask>(Chunk))->StartBackgroundTask();
            }
            else
            {
                // Synchronous generation
                Chunk->GenerateVoxelData();
            }
            
            // Add to mesh build queue
            MeshBuildQueue.Add(Chunk);
            ChunksProcessed++;
        }
    }
}

void AVoxelWorldManager::ProcessMeshBuildQueue()
{
    int32 MeshesBuilt = 0;
    TArray<AVoxelChunk*> ChunksToRequeue;

    while (MeshBuildQueue.Num() > 0 && MeshesBuilt < WorldSettings.ChunksPerFrame)
    {
        AVoxelChunk* Chunk = MeshBuildQueue[0];
        MeshBuildQueue.RemoveAt(0);

        if (!Chunk || !IsValid(Chunk))
        {
            continue;
        }

        // If chunk isn't generated yet (async still running), re-queue it
        if (!Chunk->IsGenerated())
        {
            ChunksToRequeue.Add(Chunk);
            continue;
        }

        if (Chunk->NeedsMeshRebuild())
        {
            // Update neighbors before building mesh for seamless connections
            UpdateChunkNeighbors(Chunk);
            Chunk->BuildMesh();
            MeshesBuilt++;
        }
    }

    // Re-add chunks that weren't ready yet
    for (AVoxelChunk* Chunk : ChunksToRequeue)
    {
        MeshBuildQueue.Add(Chunk);
    }
}

void AVoxelWorldManager::UpdateChunkNeighbors(AVoxelChunk* Chunk)
{
    if (!Chunk) return;

    FChunkCoord Coord = Chunk->GetChunkCoord();

    AVoxelChunk* XPos = GetChunk(FChunkCoord(Coord.X + 1, Coord.Y, Coord.Z));
    AVoxelChunk* XNeg = GetChunk(FChunkCoord(Coord.X - 1, Coord.Y, Coord.Z));
    AVoxelChunk* YPos = GetChunk(FChunkCoord(Coord.X, Coord.Y + 1, Coord.Z));
    AVoxelChunk* YNeg = GetChunk(FChunkCoord(Coord.X, Coord.Y - 1, Coord.Z));
    AVoxelChunk* ZPos = GetChunk(FChunkCoord(Coord.X, Coord.Y, Coord.Z + 1));
    AVoxelChunk* ZNeg = GetChunk(FChunkCoord(Coord.X, Coord.Y, Coord.Z - 1));

    Chunk->SetNeighbors(XPos, XNeg, YPos, YNeg, ZPos, ZNeg);
}

float AVoxelWorldManager::GetChunkDistanceFromCenter(const FChunkCoord& ChunkCoord) const
{
    // Use horizontal distance only for loading (ignore Z)
    float DX = ChunkCoord.X - CurrentLoadCenter.X;
    float DY = ChunkCoord.Y - CurrentLoadCenter.Y;
    return FMath::Sqrt(DX * DX + DY * DY);
}

void AVoxelWorldManager::SortQueueByDistance(TArray<FChunkCoord>& Queue)
{
    Queue.Sort([this](const FChunkCoord& A, const FChunkCoord& B)
    {
        return GetChunkDistanceFromCenter(A) < GetChunkDistanceFromCenter(B);
    });
}

bool AVoxelWorldManager::VoxelRaycast(const FVector& Start, const FVector& End, FVector& OutHitPosition, FVector& OutHitNormal, FVoxel& OutHitVoxel) const
{
    // DDA raycast through voxels
    FVector Direction = (End - Start).GetSafeNormal();
    float MaxDistance = (End - Start).Size();
    float VoxelSize = WorldSettings.VoxelSize;

    FVector CurrentPos = Start;
    FVector Step;
    Step.X = Direction.X >= 0 ? VoxelSize : -VoxelSize;
    Step.Y = Direction.Y >= 0 ? VoxelSize : -VoxelSize;
    Step.Z = Direction.Z >= 0 ? VoxelSize : -VoxelSize;

    // Calculate initial t values
    FVector TMax, TDelta;
    
    auto GetTMax = [&](float Pos, float Dir, float StepDir) -> float
    {
        if (FMath::Abs(Dir) < SMALL_NUMBER) return MAX_FLT;
        float Boundary = FMath::FloorToFloat(Pos / VoxelSize) * VoxelSize;
        if (StepDir > 0) Boundary += VoxelSize;
        return (Boundary - Pos) / Dir;
    };

    TMax.X = GetTMax(Start.X, Direction.X, Step.X);
    TMax.Y = GetTMax(Start.Y, Direction.Y, Step.Y);
    TMax.Z = GetTMax(Start.Z, Direction.Z, Step.Z);

    TDelta.X = FMath::Abs(Direction.X) > SMALL_NUMBER ? FMath::Abs(VoxelSize / Direction.X) : MAX_FLT;
    TDelta.Y = FMath::Abs(Direction.Y) > SMALL_NUMBER ? FMath::Abs(VoxelSize / Direction.Y) : MAX_FLT;
    TDelta.Z = FMath::Abs(Direction.Z) > SMALL_NUMBER ? FMath::Abs(VoxelSize / Direction.Z) : MAX_FLT;

    float T = 0.0f;
    int32 MaxSteps = FMath::CeilToInt(MaxDistance / VoxelSize) * 3;

    for (int32 Step_ = 0; Step_ < MaxSteps && T < MaxDistance; ++Step_)
    {
        FVoxel Voxel = GetVoxelAtWorldPosition(CurrentPos);
        
        if (Voxel.IsSolid())
        {
            OutHitPosition = CurrentPos;
            OutHitVoxel = Voxel;
            
            // Determine hit normal based on which face was hit
            if (TMax.X < TMax.Y && TMax.X < TMax.Z)
            {
                OutHitNormal = FVector(Step.X > 0 ? -1 : 1, 0, 0);
            }
            else if (TMax.Y < TMax.Z)
            {
                OutHitNormal = FVector(0, Step.Y > 0 ? -1 : 1, 0);
            }
            else
            {
                OutHitNormal = FVector(0, 0, Step.Z > 0 ? -1 : 1);
            }
            
            return true;
        }

        // Advance to next voxel
        if (TMax.X < TMax.Y)
        {
            if (TMax.X < TMax.Z)
            {
                CurrentPos.X += Step.X;
                T = TMax.X;
                TMax.X += TDelta.X;
            }
            else
            {
                CurrentPos.Z += Step.Z;
                T = TMax.Z;
                TMax.Z += TDelta.Z;
            }
        }
        else
        {
            if (TMax.Y < TMax.Z)
            {
                CurrentPos.Y += Step.Y;
                T = TMax.Y;
                TMax.Y += TDelta.Y;
            }
            else
            {
                CurrentPos.Z += Step.Z;
                T = TMax.Z;
                TMax.Z += TDelta.Z;
            }
        }
    }

    return false;
}

void AVoxelWorldManager::GetChunkStats(int32& OutLoadedChunks, int32& OutPendingChunks, int32& OutTotalVoxels) const
{
    OutLoadedChunks = LoadedChunks.Num();
    OutPendingChunks = ChunkGenerationQueue.Num() + MeshBuildQueue.Num();
    
    int32 VoxelsPerChunk = WorldSettings.ChunkSize * WorldSettings.ChunkSize * WorldSettings.ChunkSize;
    OutTotalVoxels = OutLoadedChunks * VoxelsPerChunk;
}
