// Copyright Your Company. All Rights Reserved.

#include "VoxelPlayerTracker.h"
#include "VoxelWorldManager.h"
#include "VoxelWorldModule.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

UVoxelPlayerTracker::UVoxelPlayerTracker()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;
}

void UVoxelPlayerTracker::BeginPlay()
{
    Super::BeginPlay();

    if (!VoxelWorldManager)
    {
        FindVoxelWorldManager();
    }
}

void UVoxelPlayerTracker::FindVoxelWorldManager()
{
    UWorld* World = GetWorld();
    if (!World) return;

    for (TActorIterator<AVoxelWorldManager> It(World); It; ++It)
    {
        VoxelWorldManager = *It;
        UE_LOG(LogVoxelWorld, Log, TEXT("VoxelPlayerTracker: Found VoxelWorldManager"));
        break;
    }

    if (!VoxelWorldManager)
    {
        UE_LOG(LogVoxelWorld, Warning, TEXT("VoxelPlayerTracker: No VoxelWorldManager found in level!"));
    }
}

void UVoxelPlayerTracker::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    TimeSinceLastUpdate += DeltaTime;

    if (TimeSinceLastUpdate >= UpdateInterval)
    {
        TimeSinceLastUpdate = 0.0f;

        if (!VoxelWorldManager)
        {
            FindVoxelWorldManager();
            return;
        }

        // Update load center to owner's position
        AActor* Owner = GetOwner();
        if (Owner)
        {
            FVector PlayerPosition = Owner->GetActorLocation();
            VoxelWorldManager->SetLoadCenter(PlayerPosition);

            if (bShowDebugInfo)
            {
                // Draw debug info
                FChunkCoord ChunkCoord = VoxelWorldManager->WorldToChunkCoord(PlayerPosition);
                
                int32 LoadedChunks, PendingChunks, TotalVoxels;
                VoxelWorldManager->GetChunkStats(LoadedChunks, PendingChunks, TotalVoxels);

                FString DebugText = FString::Printf(
                    TEXT("Chunk: %s\nLoaded: %d\nPending: %d\nVoxels: %d"),
                    *ChunkCoord.ToString(),
                    LoadedChunks,
                    PendingChunks,
                    TotalVoxels
                );

                DrawDebugString(GetWorld(), PlayerPosition + FVector(0, 0, 200), DebugText, nullptr, FColor::White, 0.0f);
            }
        }
    }
}
