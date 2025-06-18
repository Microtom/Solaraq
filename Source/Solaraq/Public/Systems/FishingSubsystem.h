// FishingSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FishingSubsystem.generated.h"

// Forward Declarations
class ASolaraqCharacterPawn;
class AItemActor_FishingRod;
class AFishingBobber;

UENUM(BlueprintType)
enum class EFishingState : uint8
{
	Idle,
	Casting,
	ReadyToCast,
	Fishing,
	FishHooked,
	Reeling
};

UCLASS()
class SOLARAQ_API UFishingSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// --- Public API for Tools ---
	// FIXED: Removed obsolete functions (StartCasting, ReleaseCast, StartReeling)
	void RequestPrimaryAction(ASolaraqCharacterPawn* Caster, AItemActor_FishingRod* Rod);
	void RequestPrimaryAction_Stop(ASolaraqCharacterPawn* Caster, AItemActor_FishingRod* Rod);

	// --- Callbacks from World Objects ---
	void OnToolUnequipped(AItemActor_FishingRod* Rod);
    
	// --- Getters ---
	UFUNCTION(BlueprintPure, Category = "Fishing")
	EFishingState GetCurrentState() const { return CurrentState; }

	void CatchFish();
	void RequestToggleFishingMode(ASolaraqCharacterPawn* Requester);
	void ResetState();
	void OnBobberLandedInWater();

	bool IsFishPulling() const { return bIsFishPulling; }

	/** Returns the current line tension as a value between 0.0 and 1.0. */
	UFUNCTION(BlueprintPure, Category = "Fishing|Tension")
	float GetLineTensionPercent() const;
	
protected:
	void StartFishingSequence();
    
	UFUNCTION()
	void OnFishBite();

	UFUNCTION()
	void OnFishGotAway();

	UPROPERTY(EditDefaultsOnly, Category = "Fishing|Data")
	TObjectPtr<UDataTable> FishLootTable;

private:
	

	UPROPERTY()
	EFishingState CurrentState = EFishingState::Idle;

	UPROPERTY()
	TObjectPtr<ASolaraqCharacterPawn> CurrentFisher;

	UPROPERTY()
	TObjectPtr<AItemActor_FishingRod> ActiveRod;
    
	UPROPERTY()
	TObjectPtr<AFishingBobber> ActiveBobber;
    
	FTimerHandle FishBiteTimerHandle;
	FTimerHandle HookedTimerHandle;
	float CastCharge = 0.f;

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override { return TStatId(); }
	void EnterFishingStance(ASolaraqCharacterPawn* Requester);

	// --- Tension Minigame ---
	float CurrentLineTension = 0.0f;
	float MaxLineTension = 100.0f;
	float TensionIncreaseRate = 25.0f; // How fast tension builds when reeling
	float TensionDecreaseRate = 15.0f; // How fast tension recovers naturally

	// --- Fish Behavior ---
	bool bIsFishPulling = false;
	float FishPullTensionRate = 40.0f; // Extra tension when fish is actively fighting
	FTimerHandle FishBehaviorTimerHandle;

	void UpdateTension(float DeltaTime);
	void StartFishBehavior();
	void ToggleFishBehavior();
	void OnLineSnap();
};