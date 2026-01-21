// Copyright Your Company. All Rights Reserved.

#include "VoxelDiggingTool.h"
#include "VoxelWorldManager.h"
#include "VoxelChunk.h"
#include "VoxelWorldModule.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

UVoxelDiggingTool::UVoxelDiggingTool()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void UVoxelDiggingTool::BeginPlay()
{
    Super::BeginPlay();

    if (!VoxelWorldManager)
    {
        FindVoxelWorldManager();
    }
}

void UVoxelDiggingTool::FindVoxelWorldManager()
{
    UWorld* World = GetWorld();
    if (!World) return;

    for (TActorIterator<AVoxelWorldManager> It(World); It; ++It)
    {
        VoxelWorldManager = *It;
        UE_LOG(LogVoxelWorld, Log, TEXT("VoxelDiggingTool: Found VoxelWorldManager"));
        break;
    }

    if (!VoxelWorldManager)
    {
        UE_LOG(LogVoxelWorld, Warning, TEXT("VoxelDiggingTool: No VoxelWorldManager found in level!"));
    }
}

void UVoxelDiggingTool::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update hit location for debug drawing
    FVector HitLocation, HitNormal;
    bHasValidHit = GetAimHitLocation(HitLocation, HitNormal);
    if (bHasValidHit)
    {
        LastHitLocation = HitLocation;
    }

    // Debug visualization
    if (bShowDebugSphere && bHasValidHit)
    {
        DrawDebugSphere(GetWorld(), LastHitLocation, DigRadius, 16,
            bIsDigging ? FColor::Red : (bIsBuilding ? FColor::Green : FColor::Yellow),
            false, -1.0f, 0, 2.0f);
    }

    // Handle continuous digging/building
    if (bContinuousDigging && (bIsDigging || bIsBuilding))
    {
        TimeSinceLastDig += DeltaTime;
        float DigInterval = 1.0f / DigRate;

        while (TimeSinceLastDig >= DigInterval)
        {
            TimeSinceLastDig -= DigInterval;

            if (bIsDigging)
            {
                DigFromView();
            }
            else if (bIsBuilding)
            {
                BuildFromView(BuildMaterialType);
            }
        }
    }
}

bool UVoxelDiggingTool::GetViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
    AActor* Owner = GetOwner();
    if (!Owner) return false;

    // Try to get view from player controller first
    APawn* Pawn = Cast<APawn>(Owner);
    if (Pawn)
    {
        APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
        if (PC)
        {
            FRotator ViewRotation;
            PC->GetPlayerViewPoint(OutLocation, ViewRotation);
            OutDirection = ViewRotation.Vector();
            return true;
        }
    }

    // Try to find camera component
    UCameraComponent* Camera = Owner->FindComponentByClass<UCameraComponent>();
    if (Camera)
    {
        OutLocation = Camera->GetComponentLocation();
        OutDirection = Camera->GetForwardVector();
        return true;
    }

    // Fallback to actor location and forward vector
    OutLocation = Owner->GetActorLocation();
    OutDirection = Owner->GetActorForwardVector();
    return true;
}

bool UVoxelDiggingTool::GetAimHitLocation(FVector& OutHitLocation, FVector& OutHitNormal) const
{
    if (!VoxelWorldManager) return false;

    FVector ViewLocation, ViewDirection;
    if (!GetViewPoint(ViewLocation, ViewDirection))
    {
        return false;
    }

    FVector TraceEnd = ViewLocation + ViewDirection * MaxDigDistance;

    FVoxel HitVoxel;
    return VoxelWorldManager->VoxelRaycast(ViewLocation, TraceEnd, OutHitLocation, OutHitNormal, HitVoxel);
}

bool UVoxelDiggingTool::DigAtPosition(FVector WorldPosition, float Radius, float Strength)
{
    if (Radius <= 0.0f) Radius = DigRadius;
    if (Strength <= 0.0f) Strength = DigStrength;

    bool bSuccess = ModifyTerrainSphere(WorldPosition, Radius, Strength, false);

    if (bSuccess)
    {
        OnTerrainDug.Broadcast(WorldPosition);
    }

    return bSuccess;
}

bool UVoxelDiggingTool::BuildAtPosition(FVector WorldPosition, float Radius, float Strength, EVoxelType MaterialType)
{
    if (Radius <= 0.0f) Radius = DigRadius;
    if (Strength <= 0.0f) Strength = DigStrength;

    bool bSuccess = ModifyTerrainSphere(WorldPosition, Radius, Strength, true, MaterialType);

    if (bSuccess)
    {
        OnTerrainBuilt.Broadcast(WorldPosition);
    }

    return bSuccess;
}

bool UVoxelDiggingTool::DigFromView()
{
    FVector HitLocation, HitNormal;
    if (GetAimHitLocation(HitLocation, HitNormal))
    {
        FVector DigPosition = HitLocation - HitNormal * (DigRadius * 0.3f);
        return DigAtPosition(DigPosition);
    }
    return false;
}

bool UVoxelDiggingTool::BuildFromView(EVoxelType MaterialType)
{
    FVector HitLocation, HitNormal;
    if (GetAimHitLocation(HitLocation, HitNormal))
    {
        FVector BuildPosition = HitLocation + HitNormal * (DigRadius * 0.5f);
        return BuildAtPosition(BuildPosition, -1.0f, -1.0f, MaterialType);
    }
    return false;
}

void UVoxelDiggingTool::StartDigging()
{
    bIsDigging = true;
    bIsBuilding = false;
    TimeSinceLastDig = 1.0f / DigRate;
}

void UVoxelDiggingTool::StopDigging()
{
    bIsDigging = false;
}

void UVoxelDiggingTool::StartBuilding(EVoxelType MaterialType)
{
    bIsBuilding = true;
    bIsDigging = false;
    BuildMaterialType = MaterialType;
    TimeSinceLastDig = 1.0f / DigRate;
}

void UVoxelDiggingTool::StopBuilding()
{
    bIsBuilding = false;
}

TArray<AVoxelChunk*> UVoxelDiggingTool::GetAffectedChunks(const FVector& WorldPosition, float Radius) const
{
    TArray<AVoxelChunk*> AffectedChunks;

    if (!VoxelWorldManager) return AffectedChunks;

    FVector MinBound = WorldPosition - FVector(Radius);
    FVector MaxBound = WorldPosition + FVector(Radius);

    FChunkCoord MinChunk = VoxelWorldManager->WorldToChunkCoord(MinBound);
    FChunkCoord MaxChunk = VoxelWorldManager->WorldToChunkCoord(MaxBound);

    for (int32 X = MinChunk.X; X <= MaxChunk.X; ++X)
    {
        for (int32 Y = MinChunk.Y; Y <= MaxChunk.Y; ++Y)
        {
            for (int32 Z = MinChunk.Z; Z <= MaxChunk.Z; ++Z)
            {
                AVoxelChunk* Chunk = VoxelWorldManager->GetChunk(FChunkCoord(X, Y, Z));
                if (Chunk && Chunk->IsGenerated() && Chunk->HasVoxelData())
                {
                    AffectedChunks.Add(Chunk);
                }
            }
        }
    }

    return AffectedChunks;
}

bool UVoxelDiggingTool::ModifyTerrainSphere(const FVector& WorldPosition, float Radius, float Strength, bool bAdd, EVoxelType MaterialType)
{
    if (!VoxelWorldManager)
    {
        UE_LOG(LogVoxelWorld, Warning, TEXT("DiggingTool: No VoxelWorldManager!"));
        return false;
    }

    TArray<AVoxelChunk*> AffectedChunks = GetAffectedChunks(WorldPosition, Radius);

    if (AffectedChunks.Num() == 0)
    {
        UE_LOG(LogVoxelWorld, Verbose, TEXT("DiggingTool: No chunks affected at %s"), *WorldPosition.ToString());
        return false;
    }

    bool bModifiedAny = false;

    for (AVoxelChunk* Chunk : AffectedChunks)
    {
        if (!Chunk) continue;

        FVector ChunkWorldPos = Chunk->GetActorLocation();
        FVector LocalPosition = WorldPosition - ChunkWorldPos;

        // Modify the terrain density
        Chunk->ModifyTerrain(LocalPosition, Radius, Strength, bAdd);

        // Queue the chunk for mesh rebuild - THIS IS THE KEY FIX!
        VoxelWorldManager->QueueChunkForRebuild(Chunk);

        bModifiedAny = true;
    }

    if (bModifiedAny)
    {
        UE_LOG(LogVoxelWorld, Verbose, TEXT("DiggingTool: Modified %d chunks"), AffectedChunks.Num());
    }

    return bModifiedAny;
}
