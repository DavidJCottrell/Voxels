// Copyright Your Company. All Rights Reserved.

#include "VoxelWorldManager.h"
#include "VoxelChunk.h"
#include "VoxelTerrainGenerator.h"
#include "VoxelWorldModule.h"
#include "Async/Async.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

// ==========================================
// Async Task Implementation
// ==========================================

void FChunkGenerationTask::DoWork()
{
    // Check cancellation flag before doing any work
    if (CancelFlag && *CancelFlag)
    {
        return;
    }

    // Check if chunk is still valid
    if (!Chunk || !IsValid(Chunk) || Chunk->IsPendingKillOrUnreachable())
    {
        return;
    }

    // Generate the voxel data
    Chunk->GenerateVoxelData();
}

// ==========================================
// Constructor / Destructor
// ==========================================

AVoxelWorldManager::AVoxelWorldManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.0f; // Tick every frame

    bCancelAsyncTasks = false;
}

AVoxelWorldManager::~AVoxelWorldManager()
{
    // Ensure async tasks are cancelled
    bCancelAsyncTasks = true;
}

// ==========================================
// Lifecycle
// ==========================================

void AVoxelWorldManager::BeginPlay()
{
    Super::BeginPlay();

    // Clear any editor preview chunks when starting play
    if (bIsEditorPreview)
    {
        DestroyAllChunks();
        bIsEditorPreview = false;
        bIsInitialized = false;
    }

    // Reset cancellation flag
    bCancelAsyncTasks = false;

    // Auto-initialize if not done manually
    if (!bIsInitialized)
    {
        InitializeWorld();
    }
}

void AVoxelWorldManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UE_LOG(LogVoxelWorld, Log, TEXT("VoxelWorldManager EndPlay - cleaning up..."));

    // Signal all async tasks to cancel
    bCancelAsyncTasks = true;

    // Wait for async tasks to complete
    WaitForAsyncTasks();

    // Clean up all chunks
    DestroyAllChunks();

    Super::EndPlay(EndPlayReason);

    UE_LOG(LogVoxelWorld, Log, TEXT("VoxelWorldManager cleanup complete"));
}

void AVoxelWorldManager::WaitForAsyncTasks()
{
    // Wait for all async tasks to finish (with timeout)
    const float TimeoutSeconds = 5.0f;
    const float StartTime = FPlatformTime::Seconds();

    while (ActiveAsyncTasks.Load() > 0)
    {
        if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
        {
            UE_LOG(LogVoxelWorld, Warning, TEXT("Timeout waiting for async tasks to complete. %d tasks still running."), ActiveAsyncTasks.Load());
            break;
        }

        // Give some time for tasks to complete
        FPlatformProcess::Sleep(0.01f);
    }
}

void AVoxelWorldManager::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

#if WITH_EDITOR
    if (bEnableEditorPreview && IsInEditorPreviewMode())
    {
        if (!bIsInitialized)
        {
            InitializeEditorPreview();
        }
    }
    else if (!bEnableEditorPreview && bIsEditorPreview)
    {
        ClearEditorPreview();
    }
#endif
}

bool AVoxelWorldManager::ShouldTickIfViewportsOnly() const
{
    return bEnableEditorPreview && bIsEditorPreview;
}

#if WITH_EDITOR
void AVoxelWorldManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    FName PropertyName = (PropertyChangedEvent.Property != nullptr)
        ? PropertyChangedEvent.Property->GetFName()
        : NAME_None;

    if (PropertyName == GET_MEMBER_NAME_CHECKED(AVoxelWorldManager, bEnableEditorPreview))
    {
        if (bEnableEditorPreview)
        {
            RegenerateEditorPreview();
        }
        else
        {
            ClearEditorPreview();
        }
        return;
    }

    if (PropertyName == GET_MEMBER_NAME_CHECKED(AVoxelWorldManager, EditorPreviewDistance))
    {
        if (bEnableEditorPreview && bAutoRegeneratePreview)
        {
            RegenerateEditorPreview();
        }
        return;
    }

    if (PropertyChangedEvent.MemberProperty != nullptr)
    {
        FName MemberName = PropertyChangedEvent.MemberProperty->GetFName();
        if (MemberName == GET_MEMBER_NAME_CHECKED(AVoxelWorldManager, WorldSettings))
        {
            if (bEnableEditorPreview && bAutoRegeneratePreview)
            {
                RegenerateEditorPreview();
            }
        }
    }
}

bool AVoxelWorldManager::IsInEditorPreviewMode() const
{
    UWorld* World = GetWorld();
    if (!World) return false;

    return World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview;
}

void AVoxelWorldManager::InitializeEditorPreview()
{
    if (bIsInitialized) return;

    UE_LOG(LogVoxelWorld, Log, TEXT("Initializing editor preview..."));

    TerrainGenerator = NewObject<UVoxelTerrainGenerator>(this);
    TerrainGenerator->Initialize(WorldSettings);

    CurrentLoadCenter = WorldToChunkCoord(GetActorLocation());
    bIsInitialized = true;
    bIsEditorPreview = true;
    bCancelAsyncTasks = false;

    UpdateChunkLoading();
}
#endif

void AVoxelWorldManager::RegenerateEditorPreview()
{
#if WITH_EDITOR
    if (!IsInEditorPreviewMode())
    {
        UE_LOG(LogVoxelWorld, Warning, TEXT("Cannot regenerate preview - not in editor mode"));
        return;
    }

    UE_LOG(LogVoxelWorld, Log, TEXT("Regenerating editor preview..."));

    bCancelAsyncTasks = true;
    WaitForAsyncTasks();

    DestroyAllChunks();
    bIsInitialized = false;
    bIsEditorPreview = false;

    if (bEnableEditorPreview)
    {
        bCancelAsyncTasks = false;
        InitializeEditorPreview();
    }
#endif
}

void AVoxelWorldManager::ClearEditorPreview()
{
#if WITH_EDITOR
    UE_LOG(LogVoxelWorld, Log, TEXT("Clearing editor preview..."));

    bCancelAsyncTasks = true;
    WaitForAsyncTasks();

    DestroyAllChunks();
    bIsInitialized = false;
    bIsEditorPreview = false;
#endif
}

// ==========================================
// Initialization
// ==========================================

int32 AVoxelWorldManager::GetEffectiveRenderDistance() const
{
#if WITH_EDITOR
    if (bIsEditorPreview)
    {
        return EditorPreviewDistance;
    }
#endif
    return WorldSettings.RenderDistance;
}

void AVoxelWorldManager::InitializeWorld()
{
    if (bIsInitialized)
    {
        UE_LOG(LogVoxelWorld, Warning, TEXT("World already initialized!"));
        return;
    }

    TerrainGenerator = NewObject<UVoxelTerrainGenerator>(this);
    TerrainGenerator->Initialize(WorldSettings);

    CurrentLoadCenter = FChunkCoord(0, 0, 0);
    bIsInitialized = true;
    bIsEditorPreview = false;
    bCancelAsyncTasks = false;

    UE_LOG(LogVoxelWorld, Log, TEXT("Voxel World initialized - Seed: %d, ChunkSize: %d, RenderDist: %d, LOD0: %d, LOD1: %d, LOD2: %d, CollisionDist: %d"),
        WorldSettings.Seed,
        WorldSettings.ChunkSize,
        WorldSettings.RenderDistance,
        WorldSettings.LODSettings.LOD0Distance,
        WorldSettings.LODSettings.LOD1Distance,
        WorldSettings.LODSettings.LOD2Distance,
        WorldSettings.LODSettings.CollisionDistance);

    UpdateChunkLoading();
}

// ==========================================
// Tick
// ==========================================

void AVoxelWorldManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsInitialized || bCancelAsyncTasks)
    {
        return;
    }

    // Process queues
    ProcessGenerationQueue();
    ProcessMeshBuildQueue();

    // Periodic LOD updates (not every frame)
    LODUpdateTimer += DeltaTime;
    if (LODUpdateTimer >= LODUpdateInterval)
    {
        LODUpdateTimer = 0.0f;
        UpdateChunkLODs();
    }

    // Periodic collision updates
    CollisionUpdateTimer += DeltaTime;
    if (CollisionUpdateTimer >= CollisionUpdateInterval)
    {
        CollisionUpdateTimer = 0.0f;
        UpdateChunkCollisions();
    }
}

// ==========================================
// Load Center and Chunk Loading
// ==========================================

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

    FVector RelativePos = WorldPosition - FVector(
        OutChunkCoord.X * ChunkWorldSize,
        OutChunkCoord.Y * ChunkWorldSize,
        OutChunkCoord.Z * ChunkWorldSize
    );

    OutLocalX = FMath::FloorToInt(RelativePos.X / WorldSettings.VoxelSize);
    OutLocalY = FMath::FloorToInt(RelativePos.Y / WorldSettings.VoxelSize);
    OutLocalZ = FMath::FloorToInt(RelativePos.Z / WorldSettings.VoxelSize);

    OutLocalX = FMath::Clamp(OutLocalX, 0, WorldSettings.ChunkSize - 1);
    OutLocalY = FMath::Clamp(OutLocalY, 0, WorldSettings.ChunkSize - 1);
    OutLocalZ = FMath::Clamp(OutLocalZ, 0, WorldSettings.ChunkSize - 1);
}

void AVoxelWorldManager::UpdateChunkLoading()
{
    if (bCancelAsyncTasks)
    {
        return;
    }

    int32 RenderDist = GetEffectiveRenderDistance();
    int32 HeightChunks = WorldSettings.WorldHeightChunks;

    TSet<FChunkCoord> ChunksToKeep;

    // Determine which chunks should be loaded
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

    // Sort generation queue by distance (load closest first)
    SortQueueByDistance(ChunkGenerationQueue);

    // Unload/recycle chunks that are too far away
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
        RecycleChunk(Coord);
    }
}

// ==========================================
// LOD Management
// ==========================================

void AVoxelWorldManager::UpdateChunkLODs()
{
    for (auto& Pair : LoadedChunks)
    {
        if (!Pair.Value || !IsValid(Pair.Value))
        {
            continue;
        }

        float Distance = GetChunkDistanceFromCenter(Pair.Key);
        EVoxelLOD NewLOD = GetLODForDistance(Distance);

        if (Pair.Value->GetCurrentLOD() != NewLOD)
        {
            Pair.Value->SetLOD(NewLOD);

            // Add to mesh rebuild queue
            if (!MeshBuildQueue.Contains(Pair.Value))
            {
                MeshBuildQueue.Add(Pair.Value);
            }
        }
    }
}

EVoxelLOD AVoxelWorldManager::GetLODForDistance(float Distance) const
{
    return WorldSettings.LODSettings.GetLODForDistance(Distance);
}

// ==========================================
// Collision Management
// ==========================================

void AVoxelWorldManager::UpdateChunkCollisions()
{
    for (auto& Pair : LoadedChunks)
    {
        if (!Pair.Value || !IsValid(Pair.Value))
        {
            continue;
        }

        float Distance = GetChunkDistanceFromCenter(Pair.Key);
        bool bShouldHaveCollision = WorldSettings.LODSettings.ShouldHaveCollision(Distance);

        if (Pair.Value->IsCollisionEnabled() != bShouldHaveCollision)
        {
            Pair.Value->SetCollisionEnabled(bShouldHaveCollision);

            // Need to rebuild mesh with or without collision
            if (Pair.Value->IsGenerated() && !MeshBuildQueue.Contains(Pair.Value))
            {
                MeshBuildQueue.Add(Pair.Value);
            }
        }
    }
}

// ==========================================
// Chunk Creation / Destruction / Pooling
// ==========================================

AVoxelChunk* AVoxelWorldManager::GetChunk(const FChunkCoord& ChunkCoord) const
{
    const TObjectPtr<AVoxelChunk>* ChunkPtr = LoadedChunks.Find(ChunkCoord);
    return ChunkPtr ? *ChunkPtr : nullptr;
}

AVoxelChunk* AVoxelWorldManager::CreateOrGetChunk(const FChunkCoord& ChunkCoord)
{
    // Check if already loaded
    if (LoadedChunks.Contains(ChunkCoord))
    {
        return LoadedChunks[ChunkCoord];
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    AVoxelChunk* Chunk = nullptr;

    // Try to get from pool first
    if (WorldSettings.bEnableChunkPooling && ChunkPool.Num() > 0)
    {
        Chunk = ChunkPool.Pop();
        if (Chunk && IsValid(Chunk))
        {
            Chunk->ResetChunk();
            UE_LOG(LogVoxelWorld, Verbose, TEXT("Reusing pooled chunk for %s"), *ChunkCoord.ToString());
        }
        else
        {
            Chunk = nullptr;
        }
    }

    // Create new chunk if pool was empty
    if (!Chunk)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;

#if WITH_EDITOR
        if (bIsEditorPreview)
        {
            SpawnParams.ObjectFlags |= RF_Transient;
        }
#endif

        Chunk = World->SpawnActor<AVoxelChunk>(AVoxelChunk::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

        if (!Chunk)
        {
            UE_LOG(LogVoxelWorld, Error, TEXT("Failed to spawn chunk for %s"), *ChunkCoord.ToString());
            return nullptr;
        }

#if WITH_EDITOR
        if (bIsEditorPreview)
        {
            Chunk->SetFlags(RF_Transient);
            Chunk->SetActorLabel(FString::Printf(TEXT("PreviewChunk_%s"), *ChunkCoord.ToString()));
        }
#endif
    }

    // Initialize chunk
    Chunk->InitializeChunk(ChunkCoord, WorldSettings, TerrainGenerator);

    // Set initial LOD and collision based on distance
    float Distance = GetChunkDistanceFromCenter(ChunkCoord);
    Chunk->SetLOD(GetLODForDistance(Distance));
    Chunk->SetCollisionEnabled(WorldSettings.LODSettings.ShouldHaveCollision(Distance));

    // Apply material if set
    if (VoxelMaterial)
    {
        UProceduralMeshComponent* MeshComp = Chunk->FindComponentByClass<UProceduralMeshComponent>();
        if (MeshComp)
        {
            MeshComp->SetMaterial(0, VoxelMaterial);
        }
    }

    LoadedChunks.Add(ChunkCoord, Chunk);
    UpdateChunkNeighbors(Chunk);

    return Chunk;
}

void AVoxelWorldManager::RecycleChunk(const FChunkCoord& ChunkCoord)
{
    TObjectPtr<AVoxelChunk>* ChunkPtr = LoadedChunks.Find(ChunkCoord);
    if (!ChunkPtr || !*ChunkPtr)
    {
        LoadedChunks.Remove(ChunkCoord);
        return;
    }

    AVoxelChunk* Chunk = *ChunkPtr;

    // Remove from mesh build queue
    MeshBuildQueue.Remove(Chunk);

    // Try to add to pool
    if (WorldSettings.bEnableChunkPooling && ChunkPool.Num() < WorldSettings.ChunkPoolSize)
    {
        Chunk->ResetChunk();
        ChunkPool.Add(Chunk);
        UE_LOG(LogVoxelWorld, Verbose, TEXT("Recycled chunk %s to pool (pool size: %d)"), *ChunkCoord.ToString(), ChunkPool.Num());
    }
    else
    {
        // Pool full - destroy chunk
        Chunk->Destroy();
    }

    LoadedChunks.Remove(ChunkCoord);
}

void AVoxelWorldManager::DestroyChunk(const FChunkCoord& ChunkCoord)
{
    TObjectPtr<AVoxelChunk>* ChunkPtr = LoadedChunks.Find(ChunkCoord);
    if (ChunkPtr && *ChunkPtr)
    {
        MeshBuildQueue.Remove(*ChunkPtr);
        (*ChunkPtr)->Destroy();
    }
    LoadedChunks.Remove(ChunkCoord);
}

void AVoxelWorldManager::DestroyAllChunks()
{
    UE_LOG(LogVoxelWorld, Log, TEXT("Destroying all chunks..."));

    // Mark all chunks for cancellation first
    for (auto& Pair : LoadedChunks)
    {
        if (Pair.Value && IsValid(Pair.Value))
        {
            Pair.Value->MarkPendingKill();
        }
    }

    // Clear queues
    ChunkGenerationQueue.Empty();
    MeshBuildQueue.Empty();

    // Destroy loaded chunks
    for (auto& Pair : LoadedChunks)
    {
        if (Pair.Value && IsValid(Pair.Value))
        {
            Pair.Value->Destroy();
        }
    }
    LoadedChunks.Empty();

    // Destroy pooled chunks
    for (auto& Chunk : ChunkPool)
    {
        if (Chunk && IsValid(Chunk))
        {
            Chunk->Destroy();
        }
    }
    ChunkPool.Empty();

    UE_LOG(LogVoxelWorld, Log, TEXT("All chunks destroyed"));
}

// ==========================================
// Queue Processing
// ==========================================

void AVoxelWorldManager::ProcessGenerationQueue()
{
    if (bCancelAsyncTasks)
    {
        return;
    }

    int32 ChunksProcessed = 0;
    int32 MaxChunksPerFrame = WorldSettings.ChunksPerFrame;

#if WITH_EDITOR
    if (bIsEditorPreview)
    {
        MaxChunksPerFrame = FMath::Max(MaxChunksPerFrame, 8);
    }
#endif

    while (ChunkGenerationQueue.Num() > 0 && ChunksProcessed < MaxChunksPerFrame)
    {
        if (bCancelAsyncTasks)
        {
            break;
        }

        FChunkCoord Coord = ChunkGenerationQueue[0];
        ChunkGenerationQueue.RemoveAt(0);

        AVoxelChunk* Chunk = CreateOrGetChunk(Coord);

        if (Chunk && !Chunk->IsGenerated())
        {
#if WITH_EDITOR
            // Always use synchronous generation in editor preview
            if (bIsEditorPreview)
            {
                Chunk->GenerateVoxelData();
            }
            else
#endif
            if (WorldSettings.bAsyncGeneration)
            {
                // Track active async task
                ++ActiveAsyncTasks;

                // Start async task
                auto* Task = new FAutoDeleteAsyncTask<FChunkGenerationTask>(Chunk, &bCancelAsyncTasks);
                Task->StartBackgroundTask();

                // Decrement counter when done (on game thread)
                AsyncTask(ENamedThreads::GameThread, [this]()
                {
                    --ActiveAsyncTasks;
                });
            }
            else
            {
                // Synchronous generation
                Chunk->GenerateVoxelData();
            }

            // Add to mesh build queue
            if (!MeshBuildQueue.Contains(Chunk))
            {
                MeshBuildQueue.Add(Chunk);
            }
            ChunksProcessed++;
        }
    }
}

void AVoxelWorldManager::ProcessMeshBuildQueue()
{
    if (bCancelAsyncTasks)
    {
        return;
    }

    int32 MeshesBuilt = 0;
    TArray<AVoxelChunk*> ChunksToRequeue;

    int32 MaxMeshesPerFrame = WorldSettings.MeshBuildsPerFrame;

#if WITH_EDITOR
    if (bIsEditorPreview)
    {
        MaxMeshesPerFrame = FMath::Max(MaxMeshesPerFrame, 8);
    }
#endif

    while (MeshBuildQueue.Num() > 0 && MeshesBuilt < MaxMeshesPerFrame)
    {
        if (bCancelAsyncTasks)
        {
            break;
        }

        AVoxelChunk* Chunk = MeshBuildQueue[0];
        MeshBuildQueue.RemoveAt(0);

        if (!Chunk || !IsValid(Chunk) || Chunk->IsPendingKillOrUnreachable())
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
            // Update neighbors before building mesh
            UpdateChunkNeighbors(Chunk);
            Chunk->BuildMesh();
            MeshesBuilt++;
        }
    }

    // Re-add chunks that weren't ready yet
    for (AVoxelChunk* Chunk : ChunksToRequeue)
    {
        if (Chunk && IsValid(Chunk) && !MeshBuildQueue.Contains(Chunk))
        {
            MeshBuildQueue.Add(Chunk);
        }
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

// ==========================================
// Distance Calculations
// ==========================================

float AVoxelWorldManager::GetChunkDistanceFromCenter(const FChunkCoord& ChunkCoord) const
{
    // Use horizontal distance only (ignore Z for LOD/loading)
    float DX = static_cast<float>(ChunkCoord.X - CurrentLoadCenter.X);
    float DY = static_cast<float>(ChunkCoord.Y - CurrentLoadCenter.Y);
    return FMath::Sqrt(DX * DX + DY * DY);
}

void AVoxelWorldManager::SortQueueByDistance(TArray<FChunkCoord>& Queue)
{
    Queue.Sort([this](const FChunkCoord& A, const FChunkCoord& B)
    {
        return GetChunkDistanceFromCenter(A) < GetChunkDistanceFromCenter(B);
    });
}

// ==========================================
// Voxel Access
// ==========================================

FVoxel AVoxelWorldManager::GetVoxelAtWorldPosition(const FVector& WorldPosition) const
{
    FChunkCoord ChunkCoord;
    int32 LocalX, LocalY, LocalZ;
    WorldToLocalVoxelCoord(WorldPosition, ChunkCoord, LocalX, LocalY, LocalZ);

    AVoxelChunk* Chunk = GetChunk(ChunkCoord);
    if (Chunk && Chunk->IsGenerated() && Chunk->HasVoxelData())
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
    if (Chunk && Chunk->IsGenerated() && Chunk->HasVoxelData())
    {
        Chunk->SetVoxel(LocalX, LocalY, LocalZ, Voxel);

        if (!MeshBuildQueue.Contains(Chunk))
        {
            MeshBuildQueue.Add(Chunk);
        }

        // Update adjacent chunks if on boundary
        auto QueueNeighborIfNeeded = [this](AVoxelChunk* Neighbor)
        {
            if (Neighbor && !MeshBuildQueue.Contains(Neighbor))
            {
                MeshBuildQueue.Add(Neighbor);
            }
        };

        if (LocalX == 0)
            QueueNeighborIfNeeded(GetChunk(FChunkCoord(ChunkCoord.X - 1, ChunkCoord.Y, ChunkCoord.Z)));
        else if (LocalX == WorldSettings.ChunkSize - 1)
            QueueNeighborIfNeeded(GetChunk(FChunkCoord(ChunkCoord.X + 1, ChunkCoord.Y, ChunkCoord.Z)));

        if (LocalY == 0)
            QueueNeighborIfNeeded(GetChunk(FChunkCoord(ChunkCoord.X, ChunkCoord.Y - 1, ChunkCoord.Z)));
        else if (LocalY == WorldSettings.ChunkSize - 1)
            QueueNeighborIfNeeded(GetChunk(FChunkCoord(ChunkCoord.X, ChunkCoord.Y + 1, ChunkCoord.Z)));

        if (LocalZ == 0)
            QueueNeighborIfNeeded(GetChunk(FChunkCoord(ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z - 1)));
        else if (LocalZ == WorldSettings.ChunkSize - 1)
            QueueNeighborIfNeeded(GetChunk(FChunkCoord(ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z + 1)));
    }
}

float AVoxelWorldManager::GetTerrainHeightAtWorldPosition(float WorldX, float WorldY) const
{
    if (!TerrainGenerator) return 0.0f;

    int32 VoxelX = FMath::FloorToInt(WorldX / WorldSettings.VoxelSize);
    int32 VoxelY = FMath::FloorToInt(WorldY / WorldSettings.VoxelSize);

    float TerrainHeight = TerrainGenerator->GetTerrainHeight(VoxelX, VoxelY);
    return TerrainHeight * WorldSettings.VoxelSize;
}

// ==========================================
// Raycasting
// ==========================================

bool AVoxelWorldManager::VoxelRaycast(const FVector& Start, const FVector& End, FVector& OutHitPosition, FVector& OutHitNormal, FVoxel& OutHitVoxel) const
{
    FVector Direction = (End - Start).GetSafeNormal();
    float MaxDistance = (End - Start).Size();
    float VoxelSize = WorldSettings.VoxelSize;

    FVector CurrentPos = Start;
    FVector Step;
    Step.X = Direction.X >= 0 ? VoxelSize : -VoxelSize;
    Step.Y = Direction.Y >= 0 ? VoxelSize : -VoxelSize;
    Step.Z = Direction.Z >= 0 ? VoxelSize : -VoxelSize;

    auto GetTMax = [&](float Pos, float Dir, float StepDir) -> float
    {
        if (FMath::Abs(Dir) < SMALL_NUMBER) return MAX_FLT;
        float Boundary = FMath::FloorToFloat(Pos / VoxelSize) * VoxelSize;
        if (StepDir > 0) Boundary += VoxelSize;
        return (Boundary - Pos) / Dir;
    };

    FVector TMax, TDelta;
    TMax.X = GetTMax(Start.X, Direction.X, Step.X);
    TMax.Y = GetTMax(Start.Y, Direction.Y, Step.Y);
    TMax.Z = GetTMax(Start.Z, Direction.Z, Step.Z);

    TDelta.X = FMath::Abs(Direction.X) > SMALL_NUMBER ? FMath::Abs(VoxelSize / Direction.X) : MAX_FLT;
    TDelta.Y = FMath::Abs(Direction.Y) > SMALL_NUMBER ? FMath::Abs(VoxelSize / Direction.Y) : MAX_FLT;
    TDelta.Z = FMath::Abs(Direction.Z) > SMALL_NUMBER ? FMath::Abs(VoxelSize / Direction.Z) : MAX_FLT;

    float T = 0.0f;
    int32 MaxSteps = FMath::CeilToInt(MaxDistance / VoxelSize) * 3;

    for (int32 StepCount = 0; StepCount < MaxSteps && T < MaxDistance; ++StepCount)
    {
        FVoxel Voxel = GetVoxelAtWorldPosition(CurrentPos);

        if (Voxel.IsSolid())
        {
            OutHitPosition = CurrentPos;
            OutHitVoxel = Voxel;

            if (TMax.X < TMax.Y && TMax.X < TMax.Z)
                OutHitNormal = FVector(Step.X > 0 ? -1 : 1, 0, 0);
            else if (TMax.Y < TMax.Z)
                OutHitNormal = FVector(0, Step.Y > 0 ? -1 : 1, 0);
            else
                OutHitNormal = FVector(0, 0, Step.Z > 0 ? -1 : 1);

            return true;
        }

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

// ==========================================
// Statistics and Performance
// ==========================================

void AVoxelWorldManager::GetChunkStats(int32& OutLoadedChunks, int32& OutPendingChunks, int32& OutTotalVoxels) const
{
    OutLoadedChunks = LoadedChunks.Num();
    OutPendingChunks = ChunkGenerationQueue.Num() + MeshBuildQueue.Num();

    int32 VoxelsPerChunk = WorldSettings.ChunkSize * WorldSettings.ChunkSize * WorldSettings.ChunkSize;
    OutTotalVoxels = OutLoadedChunks * VoxelsPerChunk;
}

float AVoxelWorldManager::GetMemoryUsageMB() const
{
    int64 TotalBytes = 0;

    for (const auto& Pair : LoadedChunks)
    {
        if (Pair.Value && IsValid(Pair.Value))
        {
            TotalBytes += Pair.Value->GetMemoryUsage();
        }
    }

    // Add pool memory
    for (const auto& Chunk : ChunkPool)
    {
        if (Chunk && IsValid(Chunk))
        {
            TotalBytes += Chunk->GetMemoryUsage();
        }
    }

    return static_cast<float>(TotalBytes) / (1024.0f * 1024.0f);
}

void AVoxelWorldManager::ForceCleanup()
{
    UE_LOG(LogVoxelWorld, Log, TEXT("ForceCleanup called - compacting memory..."));

    // Compact memory in all loaded chunks
    for (auto& Pair : LoadedChunks)
    {
        if (Pair.Value && IsValid(Pair.Value))
        {
            Pair.Value->CompactMemory();
        }
    }

    // Clear the chunk pool entirely
    for (auto& Chunk : ChunkPool)
    {
        if (Chunk && IsValid(Chunk))
        {
            Chunk->Destroy();
        }
    }
    ChunkPool.Empty();

    // Request garbage collection
    if (GEngine)
    {
        GEngine->ForceGarbageCollection(true);
    }

    UE_LOG(LogVoxelWorld, Log, TEXT("ForceCleanup complete - Memory usage: %.2f MB"), GetMemoryUsageMB());
}
