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
}

void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
    // You could initialize the inventory with a default size here if you want
    // For now, it will be dynamic.
}

int32 UInventoryComponent::AddItem(UItemDataAssetBase* ItemToAdd, int32 Quantity)
{
    if (!ItemToAdd || Quantity <= 0)
    {
        return Quantity; // Return original quantity if item or quantity is invalid
    }

    int32 QuantityRemaining = Quantity;

    // 1. If the item is stackable, try to add to existing stacks first.
    if (ItemToAdd->bIsStackable)
    {
        for (FInventorySlot& Slot : Items)
        {
            if (!Slot.IsEmpty() && Slot.ItemData == ItemToAdd)
            {
                int32 SpaceInStack = ItemToAdd->MaxStackSize - Slot.Quantity;
                if (SpaceInStack > 0)
                {
                    int32 AmountToAdd = FMath::Min(QuantityRemaining, SpaceInStack);
                    Slot.Quantity += AmountToAdd;
                    QuantityRemaining -= AmountToAdd;

                    if (QuantityRemaining <= 0)
                    {
                        OnInventoryUpdated.Broadcast();
                        return 0; // All items added
                    }
                }
            }
        }
    }

    // 2. Add remaining quantity to new slots.
    while (QuantityRemaining > 0)
    {
        FInventorySlot* EmptySlot = Items.FindByPredicate([](const FInventorySlot& Slot) {
            return Slot.IsEmpty();
        });

        if (EmptySlot) // Found an existing, but empty, slot
        {
            EmptySlot->ItemData = ItemToAdd;
            int32 AmountToAdd = ItemToAdd->bIsStackable ? FMath::Min(QuantityRemaining, ItemToAdd->MaxStackSize) : 1;
            EmptySlot->Quantity = AmountToAdd;
            QuantityRemaining -= AmountToAdd;
        }
        else // No empty slots, add a new one to the array
        {
            int32 AmountToAdd = ItemToAdd->bIsStackable ? FMath::Min(QuantityRemaining, ItemToAdd->MaxStackSize) : 1;
            Items.Emplace(ItemToAdd, AmountToAdd);
            QuantityRemaining -= AmountToAdd;
        }

        // If the item isn't stackable, we add one and loop again for the remaining quantity
        if (!ItemToAdd->bIsStackable && QuantityRemaining > 0)
        {
             continue;
        }
    }

    OnInventoryUpdated.Broadcast();
    return QuantityRemaining; // Should be 0 if all was added
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