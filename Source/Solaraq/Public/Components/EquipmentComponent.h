// EquipmentComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EquipmentComponent.generated.h"

class UItemDataAssetBase;
// Forward declare the classes we will be referencing
class AItemActorBase;
class UItemDataAssetBase;
class ASolaraqCharacterPawn;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SOLARAQ_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:    
	UEquipmentComponent();

	// The core function to equip an item from its data asset
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void EquipItem(UItemDataAssetBase* ItemToEquip);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void UnequipItem();

	// --- Input Handling ---
	// Called from the Player Controller to initiate an action
	void HandlePrimaryUse();
	void HandlePrimaryUse_Stop();
	void HandleSecondaryUse();
	void HandleSecondaryUse_Stop();

protected:
	virtual void BeginPlay() override;

	// A reference to the currently equipped item's spawned actor
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<AItemActorBase> CurrentEquippedActor;

	// A reference to the data asset of the currently equipped item
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UItemDataAssetBase> CurrentEquippedItemData;

private:
	// Helper to get the owning pawn
	ASolaraqCharacterPawn* GetOwnerPawn() const;
};