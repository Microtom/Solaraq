// InventoryComponent.cpp
#include "Items/InventoryComponent.h"
#include "Items/SolaraqEquipmentComponent.h" 
#include "Items/ItemConsumableDataAsset.h"
#include "Items/ItemDataAssetBase.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/SolaraqLogChannels.h" // Your log channels
#include "Pawns/SolaraqCharacterPawn.h"
#include "Pawns/SolaraqShipBase.h"

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    InventoryWidth = 5;  
    InventoryHeight = 7;
}

bool UInventoryComponent::MoveItem(const FGuid& ItemID, FIntPoint NewTopLeft)
{
    // Find the item by its ID
    FPlacedItem* ItemToMove = PlacedItems.FindByPredicate([&ItemID](const FPlacedItem& Item) {
        return Item.ItemID == ItemID;
    });

    if (!ItemToMove || !ItemToMove->ItemData)
    {
        UE_LOG(LogTemp, Warning, TEXT("MoveItem failed: Could not find item with ID %s"), *ItemID.ToString());
        return false;
    }

    // --- Check if the target space is free ---
    // To do this correctly, we must check for collisions *excluding the item we are currently moving*.
    // The simplest way is to temporarily remove the item from the list, check for free space, and then add it back.
    
    FPlacedItem CopyOfItem = *ItemToMove; // Make a copy of the item's data.
    const int32 OriginalIndex = PlacedItems.Find(*ItemToMove);
    PlacedItems.RemoveAt(OriginalIndex, 1, false); // Temporarily remove it from the array.

    // Now, check if the desired area is free.
    const bool bSpaceIsFree = IsAreaFree(NewTopLeft, CopyOfItem.ItemData->Dimensions);

    if (bSpaceIsFree)
    {
        // Success! The space is free. Update the item's position and add it back to the list.
        CopyOfItem.TopLeft = NewTopLeft;
        PlacedItems.Emplace(CopyOfItem); // Add the modified item back.
        
        UE_LOG(LogTemp, Log, TEXT("Moved item '%s' to %s"), *CopyOfItem.ItemData->DisplayName.ToString(), *NewTopLeft.ToString());
        OnInventoryUpdated.Broadcast(); // Notify the UI to refresh.
        return true;
    }
    else
    {
        // Failure. The space is occupied or out of bounds. Add the original item back to where it was.
        PlacedItems.Insert(CopyOfItem, OriginalIndex);
        UE_LOG(LogTemp, Warning, TEXT("Move failed: Target space at %s is not free."), *NewTopLeft.ToString());
        // No need to broadcast, as nothing actually changed.
        return false;
    }
}

bool UInventoryComponent::CanMoveItemTo(const FGuid& ItemID, FIntPoint NewTopLeft)
{
    FPlacedItem* ItemToMove = PlacedItems.FindByPredicate([&ItemID](const FPlacedItem& Item) {
        return Item.ItemID == ItemID;
    });

    if (!ItemToMove || !ItemToMove->ItemData)
    {
        return false;
    }

    // Temporarily remove the item to check for space
    const int32 OriginalIndex = PlacedItems.Find(*ItemToMove);
    FPlacedItem CopyOfItem = *ItemToMove;
    PlacedItems.RemoveAt(OriginalIndex, 1, false);

    const bool bSpaceIsFree = IsAreaFree(NewTopLeft, CopyOfItem.ItemData->Dimensions);

    // IMPORTANT: Add the item back to restore the original state of the inventory
    PlacedItems.Insert(CopyOfItem, OriginalIndex);

    return bSpaceIsFree;
}

bool UInventoryComponent::TransferItemTo(UInventoryComponent* TargetInventory, const FGuid& ItemID, FIntPoint TargetPos)
{
    if (!TargetInventory || TargetInventory == this) return false;

    // 1. Find the item in this inventory
    const int32 Index = PlacedItems.IndexOfByPredicate([&](const FPlacedItem& Item){ return Item.ItemID == ItemID; });
    if (Index == INDEX_NONE) return false;

    FPlacedItem ItemToTransfer = PlacedItems[Index];

    // 2. Check if the Target location is free in the TargetInventory
    // We use the helper we already have in the target component
    // Note: IsAreaFree is private, so we might need to expose it or use AddItemAt
    
    // Attempt to add it to the target at the specific location
    if (TargetInventory->AddItemAt(ItemToTransfer, TargetPos))
    {
        // 3. Success! Remove it from this inventory
        RemoveItem(ItemID, ItemToTransfer.Quantity);
        
        UE_LOG(LogTemp, Log, TEXT("Transferred item '%s' to external inventory."), *ItemToTransfer.ItemData->DisplayName.ToString());
        return true;
    }

    UE_LOG(LogTemp, Warning, TEXT("Transfer failed: Target location occupied or invalid."));
    return false;
}

void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
    // You could initialize the inventory with a default size here if you want
    // For now, it will be dynamic.
}


bool UInventoryComponent::IsAreaFree(FIntPoint TopLeft, FIntPoint Dimensions) const
{
    // The rectangle for the item we want to place.
    const FIntRect NewItemRect = FIntRect(TopLeft, TopLeft + Dimensions);

    // Check against inventory bounds first.
    if (NewItemRect.Min.X < 0 || NewItemRect.Min.Y < 0 ||
        NewItemRect.Max.X > InventoryWidth || NewItemRect.Max.Y > InventoryHeight)
    {
        return false;
    }

    // Check for overlap with all already placed items.
    for (const FPlacedItem& PlacedItem : PlacedItems)
    {
        FIntRect ExistingItemRect = FIntRect(PlacedItem.TopLeft, PlacedItem.TopLeft + PlacedItem.ItemData->Dimensions);
        if (NewItemRect.Intersect(ExistingItemRect))
        {
            return false; // Found an overlap!
        }
    }
    
    return true; // No overlaps found, the area is free.
}

bool UInventoryComponent::FindFreeSpot(FIntPoint Dimensions, FIntPoint& OutTopLeft) const
{
    // Iterate through every possible top-left cell.
    for (int32 y = 0; y <= InventoryHeight - Dimensions.Y; ++y)
    {
        for (int32 x = 0; x <= InventoryWidth - Dimensions.X; ++x)
        {
            FIntPoint CurrentTopLeft(x, y);
            if (IsAreaFree(CurrentTopLeft, Dimensions))
            {
                OutTopLeft = CurrentTopLeft;
                return true; // Found a spot!
            }
        }
    }

    return false; // No free spot of the required size was found.
}

int32 UInventoryComponent::AddItem(UItemDataAssetBase* ItemToAdd, int32 Quantity)
{
    UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] AddItem Called. Item: %s, Quantity: %d"), 
        ItemToAdd ? *ItemToAdd->GetName() : TEXT("NULL"), 
        Quantity);

    if (!ItemToAdd || Quantity <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[DEBUG_INV] AddItem Aborted: Invalid Item or Quantity <= 0"));
        return Quantity;
    }

    int32 QuantityRemaining = Quantity;

    // 1. Try to stack on existing items first.
    if (ItemToAdd->bIsStackable)
    {
        for (FPlacedItem& Item : PlacedItems)
        {
            if (Item.ItemData == ItemToAdd && Item.Quantity < ItemToAdd->MaxStackSize)
            {
                const int32 SpaceInStack = ItemToAdd->MaxStackSize - Item.Quantity;
                const int32 AmountToAdd = FMath::Min(QuantityRemaining, SpaceInStack);
                
                UE_LOG(LogTemp, Log, TEXT("[DEBUG_INV] Stacking %d items onto existing stack at %s"), AmountToAdd, *Item.TopLeft.ToString());
                
                Item.Quantity += AmountToAdd;
                QuantityRemaining -= AmountToAdd;

                if (QuantityRemaining <= 0)
                {
                    UE_LOG(LogTemp, Log, TEXT("[DEBUG_INV] All items stacked. Broadcasting Update."));
                    OnInventoryUpdated.Broadcast();
                    return 0; // All items stacked.
                }
            }
        }
    }
    
    // 2. Place remaining quantity into new slots.
    const FIntPoint ItemDimensions = ItemToAdd->Dimensions;
    UE_LOG(LogTemp, Log, TEXT("[DEBUG_INV] Attempting to place new item. Dimensions: %s. Remaining Qty: %d"), *ItemDimensions.ToString(), QuantityRemaining);

    while (QuantityRemaining > 0)
    {
        FIntPoint FoundSpot;
        if (FindFreeSpot(ItemDimensions, FoundSpot))
        {
            const int32 AmountToAdd = ItemToAdd->bIsStackable ? FMath::Min(QuantityRemaining, ItemToAdd->MaxStackSize) : 1;
            
            UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] FOUND SPOT at %s. Placing %d items. ItemID: %s"), 
                *FoundSpot.ToString(), AmountToAdd, *ItemToAdd->GetName());

            PlacedItems.Emplace(ItemToAdd, AmountToAdd, FoundSpot);
            QuantityRemaining -= AmountToAdd;

            if (!ItemToAdd->bIsStackable && QuantityRemaining > 0)
            {
                continue;
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[DEBUG_INV] FAILED TO FIND SPOT for item %s. Grid full or item too big."), *ItemToAdd->GetName());
            break;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] AddItem Complete. Final PlacedItems Count: %d. Broadcasting OnInventoryUpdated."), PlacedItems.Num());
    OnInventoryUpdated.Broadcast();
    return QuantityRemaining; 
}

bool UInventoryComponent::AddItemAt(const FPlacedItem& Item, FIntPoint TopLeft)
{
    if (!Item.ItemData) return false;

    // Check if space is free
    if (IsAreaFree(TopLeft, Item.ItemData->Dimensions))
    {
        FPlacedItem NewItem = Item;
        NewItem.TopLeft = TopLeft;
        
        // Ensure ID is preserved or generated as needed. 
        // If moving from equipment, we usually keep the ID or generate a new one. 
        // FPlacedItem copy constructor keeps the ID.

        PlacedItems.Add(NewItem);
        OnInventoryUpdated.Broadcast();
        return true;
    }
    
    return false;
}

void UInventoryComponent::UseItem(const FGuid& ItemID)
{
    if (!ItemID.IsValid())
	{
		return;
	}

	// Find the item by its unique ID
	FPlacedItem* ItemToUse = PlacedItems.FindByPredicate([&ItemID](const FPlacedItem& Item)
	{
		return Item.ItemID == ItemID;
	});
    
    if (!ItemToUse || !ItemToUse->ItemData)
    {
        return;
    }

    UItemDataAssetBase* ItemData = ItemToUse->ItemData;
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    
    UE_LOG(LogSolaraqSystem, Log, TEXT("Attempting to use item: %s"), *ItemData->DisplayName.ToString());

    // =========================================================================
    // EQUIPMENT LOGIC
    // =========================================================================
    
    // 1. Check if the item is designed for an equipment slot (Head, Body, etc.)
    if (ItemData->EquipmentSlot != EEquipmentSlot::None)
    {
        // 2. Try to find the Equipment Component on the owner (Player)
        if (USolaraqEquipmentComponent* EquipComp = Owner->FindComponentByClass<USolaraqEquipmentComponent>())
        {
            FPlacedItem PreviousItem;

            // 3. Attempt to Equip the item.
            // We pass *ItemToUse. The function makes a copy internally to store in the equipment map.
            bool bEquipSuccess = EquipComp->EquipItem(*ItemToUse, ItemData->EquipmentSlot, PreviousItem);

            if (bEquipSuccess)
            {
                UE_LOG(LogSolaraqSystem, Log, TEXT("Successfully equipped %s into slot %d"), 
                    *ItemData->DisplayName.ToString(), (int32)ItemData->EquipmentSlot);

                // 4. Remove the item from the Inventory (Backpack)
                // We typically remove 1 quantity.
                RemoveItem(ItemID, 1);

                // 5. Handle Swapping
                // If there was already a helmet on, EquipItem returns it in 'PreviousItem'.
                // We need to put that old helmet back into the backpack.
                if (PreviousItem.IsValid())
                {
                    AddItem(PreviousItem.ItemData, PreviousItem.Quantity);
                    UE_LOG(LogSolaraqSystem, Log, TEXT("Swapped items. Returned %s to inventory."), 
                        *PreviousItem.ItemData->DisplayName.ToString());
                }
            }
            else
            {
                UE_LOG(LogSolaraqSystem, Warning, TEXT("Failed to equip item. Slot mismatch or internal error."));
            }
        }
        else
        {
            UE_LOG(LogSolaraqSystem, Error, TEXT("Owner does not have an EquipmentComponent!"));
        }

        // If it was equipment, we are done. Return here so we don't try to 'consume' it below.
        return;
    }
    
    // Branch logic based on the item type
    switch (ItemData->ItemType)
    {
        case EItemType::Consumable:
        {
            // Safely cast to the consumable-specific data asset
            if (UItemConsumableDataAsset* ConsumableData = Cast<UItemConsumableDataAsset>(ItemData))
            {
                // Is the owner a character or a ship?
                if (ASolaraqCharacterPawn* Character = Cast<ASolaraqCharacterPawn>(Owner))
                {
                    // Placeholder for applying health. You'd call a function on your character's health component here.
                    // For example: Character->GetHealthComponent()->ApplyHealth(ConsumableData->HealthToRestore);
                    UE_LOG(LogSolaraqSystem, Log, TEXT("Applied %.1f health to character."), ConsumableData->HealthToRestore);
                }
                else if (ASolaraqShipBase* Ship = Cast<ASolaraqShipBase>(Owner))
                {
                    // Placeholder for applying ship health.
                    // For example: Ship->GetHealthComponent()->ApplyHealth(ConsumableData->ShipHealthToRestore);
                    UE_LOG(LogSolaraqSystem, Log, TEXT("Applied %.1f hull integrity to ship."), ConsumableData->ShipHealthToRestore);
                }

                if (ConsumableData->UseSound)
                {
                    UGameplayStatics::PlaySound2D(GetWorld(), ConsumableData->UseSound);
                }

                // Remove one item from the stack. This will also broadcast the update.
                RemoveItem(ItemID, 1);
            }
            break;
        }
        case EItemType::Tool:
        case EItemType::Weapon:
        case EItemType::Resource:
        case EItemType::Generic:
        case EItemType::QuestItem:
        default:
            UE_LOG(LogSolaraqSystem, Warning, TEXT("Item '%s' is of a type that has no 'Use' action."), *ItemData->DisplayName.ToString());
        break;
    }
}


void UInventoryComponent::RemoveItem(const FGuid& ItemID, int32 QuantityToRemove)
{
    // Debug log
    // UE_LOG(LogTemp, Log, TEXT("[DEBUG_INV] RemoveItem Requested. ID: %s"), *ItemID.ToString());

    if (!ItemID.IsValid() || QuantityToRemove <= 0)
    {
        return;
    }

    const int32 ItemIndex = PlacedItems.FindLastByPredicate([&ItemID](const FPlacedItem& Item)
    {
        return Item.ItemID == ItemID;
    });

    if (ItemIndex != INDEX_NONE)
    {
        FPlacedItem& Item = PlacedItems[ItemIndex];
        Item.Quantity -= QuantityToRemove;

        if (Item.Quantity <= 0)
        {
            PlacedItems.RemoveAt(ItemIndex);
            UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] RemoveItem Success. Item completely removed."));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] RemoveItem Partial. Remaining Qty: %d"), Item.Quantity);
        }

        OnInventoryUpdated.Broadcast();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[DEBUG_INV] RemoveItem FAILED. Could not find Item ID: %s in PlacedItems list!"), *ItemID.ToString());
        
        // --- DEBUG: Print all known IDs to compare ---
        for(const FPlacedItem& Existing : PlacedItems)
        {
            UE_LOG(LogTemp, Log, TEXT("   -> Existing Item: %s | ID: %s"), *Existing.ItemData->GetName(), *Existing.ItemID.ToString());
        }
    }
}

bool UInventoryComponent::HasItem(UItemDataAssetBase* ItemToFind, int32 Quantity /*= 1*/) const
{
    if (!ItemToFind || Quantity <= 0)
    {
        return false;
    }

    int32 TotalFound = 0;
    for (const FPlacedItem& Item : PlacedItems)
    {
        if (Item.ItemData == ItemToFind)
        {
            TotalFound += Item.Quantity;
        }
    }

    return TotalFound >= Quantity;
}

int32 UInventoryComponent::GetGridWidth() const
{
    return InventoryWidth;
}

int32 UInventoryComponent::GetGridHeight() const
{
    return InventoryHeight;
}

const TArray<FPlacedItem>& UInventoryComponent::GetPlacedItems() const
{
    return PlacedItems;
}