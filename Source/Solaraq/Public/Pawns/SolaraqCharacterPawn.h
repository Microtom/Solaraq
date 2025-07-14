
// SolaraqCharacterPawn.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Items/InventoryComponent.h"
#include "Components/EquipmentComponent.h" 
#include "SolaraqCharacterPawn.generated.h"

class AItemPickup;
class UCameraComponent;
class USpringArmComponent;
class UInventoryComponent;
class UEquipmentComponent;

UENUM(BlueprintType)
enum class ERejoinInterpolationType : uint8
{
	InterpTo UMETA(DisplayName = "Ease Out (VInterpTo)"),
	Linear   UMETA(DisplayName = "Linear Speed")
};

UCLASS()
class SOLARAQ_API ASolaraqCharacterPawn : public ACharacter
{
	GENERATED_BODY()

public:
	ASolaraqCharacterPawn();

	FORCEINLINE class UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	FORCEINLINE class UEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing")
	float FishingCameraRadius = 800.f;
	
	FVector GetAimDirection() const;

	FRotator GetTargetAimingRotation() const;
	void StartSmoothTurn(const FRotator& TargetRotation);
	void SetContinuousAiming(bool bEnable);

	// --- Sprinting ---
	/** Call this from your PlayerController to start sprinting. */
	void StartSprinting();
	/** Call this from your PlayerController to stop sprinting. */
	void StopSprinting();

	// --- Inventory ---
	/** The class of pickup to spawn when an item is dropped. This should be set in your Character Blueprint. */
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Inventory", meta=(DisplayName="Default Item Pickup Class"))
	TSubclassOf<AItemPickup> DefaultPickupClass;

	/**
	 * Handles the logic of dropping an item into the world. Finds a valid location and spawns a pickup actor.
	 * @param ItemData The data of the item to drop.
	 * @param Quantity The amount of the item to drop.
	 */
	UFUNCTION(BlueprintCallable, Category = "Solaraq|Inventory")
	void DropItem(UItemDataAssetBase* ItemData, int32 Quantity);
	
protected:
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Sprinting ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Sprinting")
	float NormalMaxWalkSpeed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Sprinting")
	float SprintMaxWalkSpeed = 800.f;

	UPROPERTY(ReplicatedUsing=OnRep_IsSprinting)
	bool bIsSprinting = false;

	UFUNCTION()
	void OnRep_IsSprinting();

	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprintingState);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArmComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float AimTurnInterpSpeed = 6.0f;
	

	// --- Inventory ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment") // <-- Add this
	TObjectPtr<UEquipmentComponent> EquipmentComponent;
	
 	// (Optional) A small delay once max offset is reached before forced rejoin begins.
 	// If 0, rejoin starts immediately once max offset is hit.
 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
 	float DelayBeforeForcedRejoin = 0.25f;

	
	
private:
    
public:
	virtual void Tick(float DeltaTime) override;

	void HandleMoveInput(const FVector2D& MovementVector);
	void HandleLookInput(const FVector2D& LookAxisVector);

	FORCEINLINE class USpringArmComponent* GetSpringArmComponent() const { return SpringArmComponent; }
	FORCEINLINE class UCameraComponent* GetCameraComponent() const { return CameraComponent; }

	/** True if the pawn is currently executing a programmatic turn. */
	bool bIsProgrammaticallyTurning = false;

	/** The rotation the pawn is trying to reach. */
	FRotator ProgrammaticTargetRotation;

	bool bShouldContinuouslyAim = false; 
};