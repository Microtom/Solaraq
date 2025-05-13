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


UENUM(BlueprintType)
enum class EDogfightState : uint8
{
	OffsetApproach  UMETA(DisplayName = "Offset Approach"), // Moving towards an offset point, facing movement dir
	DriftAim        UMETA(DisplayName = "Engage"),      // Coasting on momentum, facing target & shooting
	Reposition      UMETA(DisplayName = "Reposition")        // Moving away briefly to reset angle
};

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq | AI|Team")
	FGenericTeamId TeamId = FGenericTeamId(1); 

	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	// --- End Generic Team Interface ---


protected:
    //~ Begin AController Interface
    /** Called when the controller possesses a Pawn. Sets up perception binding. */
    virtual void OnPossess(APawn* InPawn) override;
    /** Called every frame. Main AI logic loop. */
    virtual void Tick(float DeltaTime) override;
    //~ End AController Interface

    /** Configuration for the Sight sense */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq | AI")
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    /** Function called when perception system updates our knowledge about actors */
    UFUNCTION() // Must be UFUNCTION to bind to delegates
    void HandlePerceptionUpdated(const TArray<AActor*>& UpdatedActors);


    // --- AI State Variables ---

    /** The current actor the AI is targeting. Weak ptr to avoid preventing destruction. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solaraq | AI State")
    TWeakObjectPtr<AActor> CurrentTargetActor;

    /** Last known location of the target (if target is lost). */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solaraq | AI State")
    FVector LastKnownTargetLocation;

    /** Location the AI should aim towards (prediction result). */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solaraq | AI State")
    FVector PredictedAimLocation;

    /** Does the AI currently have sight of the target? */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solaraq | AI State")
    bool bHasLineOfSight = false;
	
	// --- Movement Behavior Parameters ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq | AI Behavior | Dogfight")
	float DogfightOffsetDistance = 1500.0f; // How far to the side to aim during offset approach

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq | AI Behavior | Dogfight")
	float OffsetApproachDuration = 2.5f; // How long to thrust during offset approach before drifting

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq | AI Behavior | Dogfight")
	float DriftAimAngleThreshold = 80.0f; // Max angle (degrees) between velocity and target dir before repositioning

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq | AI Behavior | Dogfight")
	float RepositionDuration = 1.5f; // How long to thrust away during reposition

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq | AI Behavior | Dogfight")
	float RepositionDistance = 2000.0f; // How far away to move during reposition

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq | AI Behavior | Dogfight")
	float EngageForwardThrustScale = 0.7f;
	
    // --- Helper Functions ---
    /** Updates the CurrentTargetActor based on perception data. */
    virtual void UpdateTargetActor(const TArray<AActor*>& PerceivedActors);

    /** Reference to the controlled ship pawn */
    UPROPERTY(Transient) // Doesn't need saving or replication itself
    TObjectPtr<ASolaraqEnemyShip> ControlledEnemyShip;

	// New Dogfight State Variables
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Solaraq | AI State | Debug")
	EDogfightState CurrentDogfightState = EDogfightState::OffsetApproach; // Start by approaching

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Solaraq | AI State | Debug")
	float TimeInCurrentDogfightState = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Solaraq | AI State | Debug")
	FVector CurrentMovementTargetPoint; // Stores the offset or reposition target

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Solaraq | AI State | Debug")
	int32 CurrentOffsetSide = 1; // 1 for right, -1 for left relative to target

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq | AI Behavior | Movement")
	float DogfightRange = 5000.0f; // Max distance for dogfighting behavior

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq | AI Behavior | Movement")
	float ReversalAngleThreshold = 135.0f; // Angle (degrees) behind AI to trigger boost reversal

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq | AI Behavior | Movement")
	float BoostTurnCompletionAngle = 30.0f; // Angle (degrees) within target direction to stop boost turn
	
private:
	UPROPERTY(Transient) // Temporary state for boost turn
	bool bIsPerformingBoostTurn = false;

	// Helper to track strafe direction flipping
	float TimeSinceLastStrafeFlip = 0.0f;
	float StrafeFlipInterval = 3.0f; // How often to potentially flip strafe direction
	int8 CurrentStrafeDirection = 1; // 1 for right, -1 for left relative to target

	
	UPROPERTY(Transient) // This state doesn't need saving
	bool bShouldBoostOnNextApproach = false;
	
	void ExecuteIdleMovement();
	void ExecuteChaseMovement(const FVector& TargetLocation, float DeltaTime);
	void ExecuteDogfightMovement(AActor* Target, float DeltaTime); 
	void ExecuteReversalTurnMovement(const FVector& TargetLocation, float AngleToTarget, float DeltaTime);

	void HandleOffsetApproach(AActor* Target, float DeltaTime);
	void HandleEngage(AActor* Target, float DeltaTime); 
	void HandleReposition(AActor* Target, float DeltaTime);
	
	// Gets the angle between ship's forward and direction to target
	float GetAngleToTarget(const FVector& TargetLocation) const;
};


