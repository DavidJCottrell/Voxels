// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VoxelPlayerTracker.generated.h"

class AVoxelWorldManager;

/**
 * Component that tracks player position and updates voxel world chunk loading
 * Attach this to your player pawn/character to enable automatic chunk loading
 */
UCLASS(ClassGroup=(VoxelWorld), meta=(BlueprintSpawnableComponent))
class VOXELWORLD_API UVoxelPlayerTracker : public UActorComponent
{
    GENERATED_BODY()

public:
    UVoxelPlayerTracker();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** How often to update chunk loading (in seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel World")
    float UpdateInterval = 0.5f;

    /** Reference to the voxel world manager (auto-found if not set) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel World")
    TObjectPtr<AVoxelWorldManager> VoxelWorldManager;

    /** Enable debug visualization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowDebugInfo = false;

protected:
    virtual void BeginPlay() override;

private:
    float TimeSinceLastUpdate = 0.0f;

    void FindVoxelWorldManager();
};
