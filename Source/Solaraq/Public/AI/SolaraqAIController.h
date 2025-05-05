// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "SolaraqAIController.generated.h"

// Forward Declarations
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_AI;
class ASolaraqEnemyShip; // Forward declare your ship base

/**
 * AI Controller for Solaraq enemy ships. Uses AIPerception and C++ logic.
 */
UCLASS()
class SOLARAQ_API ASolaraqAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	ASolaraqAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// --- Generic Team Interface ---
	/** Assigns a team ID for affiliation checks */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI|Team")
	FGenericTeamId TeamId = FGenericTeamId(1); 

	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	// --- End Generic Team Interface ---
	
	/**
		 * Calculates the future intercept point for aiming a projectile.
		 * @param ShooterLocation Current location of the shooter (weapon muzzle).
		 * @param ShooterVelocity Current velocity vector of the shooter.
		 * @param TargetLocation Current location of the target.
		 * @param TargetVelocity Current velocity vector of the target.
		 * @param ProjectileSpeed Speed of the projectile (scalar, assumed constant).
		 * @param InterceptPoint (Output) The calculated world location to aim at.
		 * @return True if a valid future intercept point was found, false otherwise.
		 */
	UFUNCTION(BlueprintPure, Category = "Solaraq|AI|Prediction", meta = (DisplayName = "Calculate Intercept Point"))
	static bool CalculateInterceptPoint(
		FVector ShooterLocation,
		FVector ShooterVelocity,
		FVector TargetLocation,
		FVector TargetVelocity,
		float ProjectileSpeed,
		FVector& InterceptPoint // Output parameter
	);

protected:
    //~ Begin AController Interface
    /** Called when the controller possesses a Pawn. Sets up perception binding. */
    virtual void OnPossess(APawn* InPawn) override;
    /** Called every frame. Main AI logic loop. */
    virtual void Tick(float DeltaTime) override;
    //~ End AController Interface

    /** Configuration for the Sight sense */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    /** Function called when perception system updates our knowledge about actors */
    UFUNCTION() // Must be UFUNCTION to bind to delegates
    void HandlePerceptionUpdated(const TArray<AActor*>& UpdatedActors);


    // --- AI State Variables ---

    /** The current actor the AI is targeting. Weak ptr to avoid preventing destruction. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI State")
    TWeakObjectPtr<AActor> CurrentTargetActor;

    /** Last known location of the target (if target is lost). */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI State")
    FVector LastKnownTargetLocation;

    /** Location the AI should aim towards (prediction result). */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI State")
    FVector PredictedAimLocation;

    /** Does the AI currently have sight of the target? */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI State")
    bool bHasLineOfSight = false;
	
    // TODO: Add an enum for AI State (e.g., Idle, Searching, Attacking)
    // enum class EAIState : uint8 { Idle, Searching, Attacking };
    // EAIState CurrentAIState = EAIState::Idle;

    // --- Helper Functions ---
    /** Updates the CurrentTargetActor based on perception data. */
    virtual void UpdateTargetActor(const TArray<AActor*>& PerceivedActors);

    /** Reference to the controlled ship pawn */
    UPROPERTY(Transient) // Doesn't need saving or replication itself
    TObjectPtr<ASolaraqEnemyShip> ControlledEnemyShip;
};
