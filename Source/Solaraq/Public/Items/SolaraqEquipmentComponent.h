#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/InventoryComponent.h" // For FPlacedItem
#include "Items/ItemDataAssetBase.h"
#include "SolaraqEquipmentComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEquipmentUpdated);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SOLARAQ_API USolaraqEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	USolaraqEquipmentComponent();

	/** Tries to equip an item into a specific slot. Returns previous item if swapped, or null. */
	UFUNCTION(BlueprintCallable, Category="Equipment")
	bool EquipItem(const FPlacedItem& IncomingItem, EEquipmentSlot TargetSlot, FPlacedItem& OutPreviousItem);

	/** Removes item from slot */
	UFUNCTION(BlueprintCallable, Category="Equipment")
	bool UnequipItem(EEquipmentSlot Slot, FPlacedItem& OutRemovedItem);

	/** Gets the item currently in a specific slot */
	UFUNCTION(BlueprintPure, Category="Equipment")
	bool GetItemInSlot(EEquipmentSlot Slot, FPlacedItem& OutItem) const;

	UFUNCTION(BlueprintCallable, Category="Equipment|Input")
	void HandlePrimaryUse();

	UFUNCTION(BlueprintCallable, Category="Equipment|Input")
	void HandlePrimaryUse_Stop();

	UFUNCTION(BlueprintCallable, Category="Equipment|Input")
	void HandleSecondaryUse();

	UFUNCTION(BlueprintCallable, Category="Equipment|Input")
	void HandleSecondaryUse_Stop();
	
	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FOnEquipmentUpdated OnEquipmentUpdated;

private:
	// Map storing the item in each slot.
	UPROPERTY(VisibleAnywhere, Category="Equipment")
	TMap<EEquipmentSlot, FPlacedItem> EquippedItems;
};