// ItemActor_FishingRod.h
#pragma once

#include "CoreMinimal.h"
#include "Items/ItemActorBase.h"
#include "ProceduralMeshComponent.h"
#include "Systems/Fishing/FishingXPBD.h" // INCLUDE THE NEW FILE
#include "ItemActor_FishingRod.generated.h"

class AFishingBobber;

UCLASS()
class SOLARAQ_API AItemActor_FishingRod : public AItemActorBase
{
    GENERATED_BODY()

public:
    AItemActor_FishingRod();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // ... (Keep existing OnEquip, Interface overrides) ...
    virtual void OnEquip() override;
    virtual void OnItemDataChanged() override;
    virtual void OnUnequip() override;
    virtual void PrimaryUse() override;
    virtual void PrimaryUse_Stop() override;

    AFishingBobber* SpawnAndCastBobber(const FVector& CastDirection, float Charge);
    void StartReeling();
    void NotifyFishBite();
    void NotifyReset();
    void StopReeling();
    void NotifyBobberLanded();

    float TargetRopeLength = 0.0f;
    float CurrentRopeLength = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Rope Simulation")
    float RopeSegmentLength = 10.0f;
    
    bool IsReeling() const { return bIsReeling; }

protected:
    // ... (Keep Components: RodMesh, FishingLineMesh, etc.) ...
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    TObjectPtr<USkeletalMeshComponent> RodMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    TObjectPtr<UProceduralMeshComponent> FishingLineMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    TObjectPtr<UStaticMeshComponent> IdleBobberMesh;

    // ... (Keep Properties: Sockets, Classes, Sounds) ...
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Fishing Rod")
    FName RodTipSocketName = "RodTipSocket";
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Fishing Rod")
    TSubclassOf<AFishingBobber> BobberClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Fishing Rod")
    TObjectPtr<UMaterialInterface> FishingLineMaterial;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Fishing Rod|Audio")
    TObjectPtr<USoundBase> FishBiteSound;
    
    // ... (Keep Gameplay Properties: CastPower, ReelSpeed) ...
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Fishing Rod")
    float CastPower = 1500.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Fishing Rod")
    float ReelSpeed = 1000.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Fishing Rod")
    float FishPullSpeed = 600.f; 
    
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|FishingRod|Casting")
    float CastAngle = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solaraq|FishingRod|Physics")
    float MinCastRopeLength = 300.0f;
    
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Rope Simulation")
    float MaxRopeLength = 5000.0f;

    // XPBD SETTINGS
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|XPBD")
    int32 SolverSubSteps = 10;

    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|XPBD")
    float RopeMass = 0.5f;

    // --- Rope Rendering ---
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Rope Rendering")
    float RopeWidth = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Rope Rendering", meta=(ClampMin="3"))
    int32 RopeSides = 6;

private:
    // Replaced internal functions
    void InitializeRope();
    void DrawRope();
    
    // --- THE NEW SOLVER INSTANCE ---
    FXPBDSolver RopeSolver;

    // State management
    void ManageRopeState(float DeltaTime);

    // Mesh buffers
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;

    UPROPERTY()
    TObjectPtr<AFishingBobber> CurrentBobber;

    bool bIsCasting = false;
    bool bIsReeling = false;
    bool bBobberHasLanded = false;
};