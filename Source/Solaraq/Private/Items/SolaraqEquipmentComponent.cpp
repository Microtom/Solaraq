#include "Items/SolaraqEquipmentComponent.h"

USolaraqEquipmentComponent::USolaraqEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool USolaraqEquipmentComponent::EquipItem(const FPlacedItem& IncomingItem, EEquipmentSlot TargetSlot, FPlacedItem& OutPreviousItem)
{
	if (!IncomingItem.ItemData) return false;

	// 1. Validation (Optional here, can also be done in UI Controller)
	if (IncomingItem.ItemData->EquipmentSlot != TargetSlot)
	{
		return false; // Item type doesn't match slot
	}

	// 2. Check for existing item
	if (EquippedItems.Contains(TargetSlot))
	{
		OutPreviousItem = EquippedItems[TargetSlot];
	}
	else
	{
		OutPreviousItem = FPlacedItem(); // Empty
	}

	// 3. Equip new item (Always at 0,0 relative to the equipment slot grid)
	FPlacedItem NewEquip = IncomingItem;
	NewEquip.TopLeft = FIntPoint(0, 0); 
	EquippedItems.Add(TargetSlot, NewEquip);

	OnEquipmentUpdated.Broadcast();
	return true;
}

bool USolaraqEquipmentComponent::UnequipItem(EEquipmentSlot Slot, FPlacedItem& OutRemovedItem)
{
	if (EquippedItems.Contains(Slot))
	{
		OutRemovedItem = EquippedItems[Slot];
		EquippedItems.Remove(Slot);
		OnEquipmentUpdated.Broadcast();
		return true;
	}
	return false;
}

bool USolaraqEquipmentComponent::GetItemInSlot(EEquipmentSlot Slot, FPlacedItem& OutItem) const
{
	if (const FPlacedItem* FoundItem = EquippedItems.Find(Slot))
	{
		OutItem = *FoundItem;
		return true;
	}
	return false;
}

void USolaraqEquipmentComponent::HandlePrimaryUse()
{
	// Check if we have an item in the Main Hand
	if (FPlacedItem* MainHandItem = EquippedItems.Find(EEquipmentSlot::MainHand))
	{
		if (MainHandItem->ItemData)
		{
			UE_LOG(LogTemp, Log, TEXT("Primary Use triggered with item: %s"), *MainHandItem->ItemData->DisplayName.ToString());
            
			// TODO: Add logic here to switch on ItemType. 
			// If ItemType == Tool, cast to ToolDataAsset and call tool logic (like Fishing).
			// If ItemType == Weapon, call weapon logic.
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Primary Use triggered (No item in Main Hand)"));
	}
}

void USolaraqEquipmentComponent::HandlePrimaryUse_Stop()
{
	UE_LOG(LogTemp, Log, TEXT("Primary Use Stopped"));
}

void USolaraqEquipmentComponent::HandleSecondaryUse()
{
	// Logic for right-click / secondary action
	UE_LOG(LogTemp, Log, TEXT("Secondary Use triggered"));
}

void USolaraqEquipmentComponent::HandleSecondaryUse_Stop()
{
	UE_LOG(LogTemp, Log, TEXT("Secondary Use Stopped"));
}
