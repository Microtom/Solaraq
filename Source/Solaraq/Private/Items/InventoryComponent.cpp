// InventoryComponent.cpp
#include "Items/InventoryComponent.h"

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

    if (!ItemToMove)
    {
        // Item not found
        return false;
    }

    // --- Check if the target space is free ---
    // This is tricky: we need to check if the area is free *excluding the item we are currently moving*.
    // A simple way to do this is to temporarily remove the item, check, and then add it back.
    
    FPlacedItem CopyOfItem = *ItemToMove; // Make a copy
    int32 OriginalIndex = PlacedItems.Find(*ItemToMove);
    PlacedItems.RemoveAt(OriginalIndex); // Temporarily remove it

    bool bSpaceIsFree = IsAreaFree(NewTopLeft, CopyOfItem.ItemData->Dimensions);

    if (bSpaceIsFree)
    {
        // Success! The space is free. Update the item's position and add it back.
        CopyOfItem.TopLeft = NewTopLeft;
        PlacedItems.Emplace(CopyOfItem);
        OnInventoryUpdated.Broadcast(); // Notify the UI to refresh
        return true;
    }
    else
    {
        // Failure. The space is occupied. Add the original item back to its old spot.
        PlacedItems.Insert(CopyOfItem, OriginalIndex);
        // No need to broadcast, as nothing actually changed.
        return false;
    }
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
    if (!ItemToAdd || Quantity <= 0)
    {
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
                int32 SpaceInStack = ItemToAdd->MaxStackSize - Item.Quantity;
                int32 AmountToAdd = FMath::Min(QuantityRemaining, SpaceInStack);
                Item.Quantity += AmountToAdd;
                QuantityRemaining -= AmountToAdd;

                if (QuantityRemaining <= 0)
                {
                    OnInventoryUpdated.Broadcast();
                    return 0; // All items stacked.
                }
            }
        }
    }
    
    // 2. Place remaining quantity into new slots.
    FIntPoint ItemDimensions = ItemToAdd->Dimensions;
    while (QuantityRemaining > 0)
    {
        FIntPoint FoundSpot;
        if (FindFreeSpot(ItemDimensions, FoundSpot))
        {
            // We found a free spot. Place the item.
            int32 AmountToAdd = ItemToAdd->bIsStackable ? FMath::Min(QuantityRemaining, ItemToAdd->MaxStackSize) : 1;
            
            PlacedItems.Emplace(ItemToAdd, AmountToAdd, FoundSpot);
            QuantityRemaining -= AmountToAdd;

            // For non-stackable items, we must loop again to place the next one.
            if (!ItemToAdd->bIsStackable && QuantityRemaining > 0)
            {
                continue;
            }
        }
        else
        {
            // No more room in the inventory for an item of this size.
            break;
        }
    }

    OnInventoryUpdated.Broadcast();
    return QuantityRemaining; // Return any un-added quantity.
}

void UInventoryComponent::UseItem(int32 SlotIndex)
{
    // Validate the slot index and ensure the slot is not empty
    if (!Items.IsValidIndex(SlotIndex) || Items[SlotIndex].IsEmpty())
    {
        return;
    }

    UItemDataAssetBase* ItemData = Items[SlotIndex].ItemData;
    if (!ItemData)
    {
        return;
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    
    UE_LOG(LogSolaraqSystem, Log, TEXT("Attempting to use item: %s"), *ItemData->DisplayName.ToString());

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

                // Remove one item from the stack
                RemoveItem(ItemData, 1);
            }
                
            break;
        }

        case EItemType::Tool: // Fall through
        case EItemType::Weapon:
           
        
        // Add cases for other types here
        case EItemType::Resource:
        case EItemType::Generic:
        case EItemType::QuestItem:
        default:
            UE_LOG(LogSolaraqSystem, Warning, TEXT("Item '%s' is of a type that has no 'Use' action."), *ItemData->DisplayName.ToString());
        break;
    }
}


void UInventoryComponent::RemoveItem(UItemDataAssetBase* ItemToRemove, int32 Quantity)
{
    if (!ItemToRemove || Quantity <= 0)
    {
        return;
    }

    int32 QuantityRemainingToRemove = Quantity;

    // Iterate backwards so we can safely remove slots if they become empty
    for (int32 i = Items.Num() - 1; i >= 0; --i)
    {
        FInventorySlot& Slot = Items[i];
        if (!Slot.IsEmpty() && Slot.ItemData == ItemToRemove)
        {
            int32 AmountToRemove = FMath::Min(QuantityRemainingToRemove, Slot.Quantity);
            Slot.Quantity -= AmountToRemove;
            QuantityRemainingToRemove -= AmountToRemove;

            if (Slot.Quantity <= 0)
            {
                // Clear the slot
                Slot.ItemData = nullptr;
                Slot.Quantity = 0;
            }

            if (QuantityRemainingToRemove <= 0)
            {
                break; // All requested items have been removed
            }
        }
    }

    OnInventoryUpdated.Broadcast();
}

bool UInventoryComponent::HasItem(UItemDataAssetBase* ItemToFind, int32 Quantity /*= 1*/) const
{
    if (!ItemToFind || Quantity <= 0)
    {
        return false;
    }

    int32 TotalFound = 0;
    for (const FInventorySlot& Slot : Items)
    {
        if (!Slot.IsEmpty() && Slot.ItemData == ItemToFind)
        {
            TotalFound += Slot.Quantity;
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