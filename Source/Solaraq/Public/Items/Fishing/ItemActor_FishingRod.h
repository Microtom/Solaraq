// ItemActor_FishingRod.h
#pragma once

#include "CoreMinimal.h"
#include "Items/ItemActorBase.h"
#include "ProceduralMeshComponent.h" // <-- **THE CRITICAL INCLUDE FOR FProcMeshTangent**
#include "ItemActor_FishingRod.generated.h"

// Forward Declarations
class AFishingBobber;
class USoundBase;
class UMaterialInterface;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FVerletParticle
{
    GENERATED_BODY()
    FVector Position = FVector::ZeroVector;
    FVector OldPosition = FVector::ZeroVector;
};

UCLASS()
class SOLARAQ_API AItemActor_FishingRod : public AItemActorBase
{
    GENERATED_BODY()

public:
    AItemActor_FishingRod();
    virtual void BeginPlay() override;
    
    //~ Begin AItemActorBase Interface
    virtual void OnEquip() override;
    virtual void OnItemDataChanged() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnUnequip() override;
    virtual void PrimaryUse() override;
    virtual void PrimaryUse_Stop() override;
    //~ End AItemActorBase Interface

    // --- Public API ---
    AFishingBobber* SpawnAndCastBobber(const FVector& CastDirection, float Charge);
    void StartReeling();
    void NotifyFishBite();
    void NotifyReset();
    void StopReeling();
    void NotifyBobberLanded();
    
    // The length the rope is trying to reach
    float TargetRopeLength = 0.0f;

    // The current physical length of the rope simulation
    float CurrentRopeLength = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Rope Simulation")
    float RopeSegmentLength = 10.0f;
    
protected:
    // --- Components ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    TObjectPtr<USkeletalMeshComponent> RodMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    TObjectPtr<UProceduralMeshComponent> FishingLineMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    TObjectPtr<UStaticMeshComponent> IdleBobberMesh;

    // --- Properties ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Fishing Rod")
    FName RodTipSocketName = "RodTipSocket";

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Fishing Rod")
    TSubclassOf<AFishingBobber> BobberClass;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Fishing Rod")
    TObjectPtr<UMaterialInterface> FishingLineMaterial;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Fishing Rod|Audio")
    TObjectPtr<USoundBase> FishBiteSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Fishing Rod")
    float CastPower = 1500.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Fishing Rod")
    float ReelSpeed = 1000.f;

    // --- Rope Rendering ---
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Rope Rendering")
    float RopeWidth = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Rope Rendering", meta=(ClampMin="3"))
    int32 RopeSides = 6;

    // --- Rope Simulation ---
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Rope Simulation")
    int32 RopeSolverIterations = 8;

    
    
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Rope Simulation")
    float InitialRopeLength = 50.0f;

    // The fixed timestep for our simulation. Smaller = more stable but more expensive.
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Rope Simulation")
    float TimeStep = 0.016f; // Corresponds to ~60fps

    // How much velocity is retained each step. 1.0 = no damping, 0.9 = 10% lost.
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Rope Simulation", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Damping = 0.99f;
    
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Rope Simulation")
    float MaxRopeLength = 5000.0f; // 50 meters max

    /** The minimum length the rope will be when cast (at zero charge). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FishingRod|Physics")
    float MinCastRopeLength = 300.0f;
    
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Rope Simulation")
    float CastingSpeed = 500.0f; // How fast the line extends per second

    UPROPERTY(EditDefaultsOnly, Category = "FishingRod|Casting")
    float CastAngle = 45.0f;
    
private:
    // --- Private Functions ---
    void InitializeRope();
    void SimulateRope(float DeltaTime);
    void UpdateRopeLength(float DeltaTime);
    void DrawRope();
    
    // --- Private State ---
    TArray<FVerletParticle> RopeParticles;

    // We need an accumulator for sub-stepping
    float TimeAccumulator = 0.0f;
    
    // --- Mesh Generation Buffers ---
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;

    UPROPERTY()
    TObjectPtr<AFishingBobber> CurrentBobber;
    
    

    // Input state flags
    bool bIsCasting = false;
    bool bIsReeling = false;
    bool bIsRopeInitialized = false;
    bool bBobberHasLanded = false;
};