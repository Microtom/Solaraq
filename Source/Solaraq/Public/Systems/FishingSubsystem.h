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
};