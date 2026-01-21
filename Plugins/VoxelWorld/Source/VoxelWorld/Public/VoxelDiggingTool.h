// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VoxelTypes.h"
#include "VoxelDiggingTool.generated.h"

class AVoxelWorldManager;
class AVoxelChunk;

// Declare delegate types for events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerrainModified, FVector, Location);

/**
 * Component that provides terrain digging/building functionality
 * Attach to your player pawn/character to enable terrain modification
 */
UCLASS(ClassGroup=(VoxelWorld), meta=(BlueprintSpawnableComponent))
class VOXELWORLD_API UVoxelDiggingTool : public UActorComponent
{
    GENERATED_BODY()

public:
    UVoxelDiggingTool();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ==========================================
    // Tool Settings
    // ==========================================

    /** Radius of the digging sphere (in world units) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digging Tool", meta = (ClampMin = "50.0", ClampMax = "1000.0"))
    float DigRadius = 150.0f;

    /** Strength of digging per action (how much terrain is removed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digging Tool", meta = (ClampMin = "0.1"))
    float DigStrength = 0.5f;

    /** Maximum distance for digging raycast */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digging Tool", meta = (ClampMin = "100.0"))
    float MaxDigDistance = 1000.0f;

    /** Enable continuous digging while input is held */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digging Tool")
    bool bContinuousDigging = true;

    /** Rate of continuous digging (digs per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digging Tool", meta = (ClampMin = "1.0", EditCondition = "bContinuousDigging"))
    float DigRate = 10.0f;

    /** Reference to the voxel world manager (auto-found if not set) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digging Tool")
    TObjectPtr<AVoxelWorldManager> VoxelWorldManager;

    /** Enable debug visualization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowDebugSphere = false;

    // ==========================================
    // Digging Functions
    // ==========================================

    UFUNCTION(BlueprintCallable, Category = "Digging Tool")
    bool DigAtPosition(FVector WorldPosition, float Radius = -1.0f, float Strength = -1.0f);

    UFUNCTION(BlueprintCallable, Category = "Digging Tool")
    bool BuildAtPosition(FVector WorldPosition, float Radius = -1.0f, float Strength = -1.0f, EVoxelType MaterialType = EVoxelType::Dirt);

    UFUNCTION(BlueprintCallable, Category = "Digging Tool")
    bool DigFromView();

    UFUNCTION(BlueprintCallable, Category = "Digging Tool")
    bool BuildFromView(EVoxelType MaterialType = EVoxelType::Dirt);

    UFUNCTION(BlueprintCallable, Category = "Digging Tool")
    void StartDigging();

    UFUNCTION(BlueprintCallable, Category = "Digging Tool")
    void StopDigging();

    UFUNCTION(BlueprintCallable, Category = "Digging Tool")
    void StartBuilding(EVoxelType MaterialType = EVoxelType::Dirt);

    UFUNCTION(BlueprintCallable, Category = "Digging Tool")
    void StopBuilding();

    UFUNCTION(BlueprintCallable, Category = "Digging Tool")
    bool IsDigging() const { return bIsDigging; }

    UFUNCTION(BlueprintCallable, Category = "Digging Tool")
    bool IsBuilding() const { return bIsBuilding; }

    UFUNCTION(BlueprintCallable, Category = "Digging Tool")
    bool GetAimHitLocation(FVector& OutHitLocation, FVector& OutHitNormal) const;

    // ==========================================
    // Events
    // ==========================================

    /** Called when terrain is successfully dug */
    UPROPERTY(BlueprintAssignable, Category = "Digging Tool|Events")
    FOnTerrainModified OnTerrainDug;

    /** Called when terrain is successfully built */
    UPROPERTY(BlueprintAssignable, Category = "Digging Tool|Events")
    FOnTerrainModified OnTerrainBuilt;

protected:
    void FindVoxelWorldManager();
    bool GetViewPoint(FVector& OutLocation, FVector& OutDirection) const;
    bool ModifyTerrainSphere(const FVector& WorldPosition, float Radius, float Strength, bool bAdd, EVoxelType MaterialType = EVoxelType::Dirt);
    TArray<AVoxelChunk*> GetAffectedChunks(const FVector& WorldPosition, float Radius) const;

private:
    bool bIsDigging = false;
    bool bIsBuilding = false;
    EVoxelType BuildMaterialType = EVoxelType::Dirt;
    float TimeSinceLastDig = 0.0f;

    FVector LastHitLocation = FVector::ZeroVector;
    bool bHasValidHit = false;
};
