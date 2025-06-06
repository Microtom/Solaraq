// InventoryComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

// Forward declaration
class UItemDataAssetBase;

// Represents a single slot in the inventory. It links an item type (DataAsset) with a quantity.
USTRUCT(BlueprintType)
struct FInventorySlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    TObjectPtr<UItemDataAssetBase> ItemData = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "0"))
    int32 Quantity = 0;

    // Helper to quickly check if the slot is empty
    bool IsEmpty() const { return ItemData == nullptr || Quantity <= 0; }
    
    // Default constructor
    FInventorySlot() {}

    // Convenience constructor
    FInventorySlot(UItemDataAssetBase* InItemData, int32 InQuantity)
        : ItemData(InItemData), Quantity(InQuantity) {}
};


// Delegate to notify other systems (like the UI) that the inventory has changed.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SOLARAQ_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInventoryComponent();

    // The main function to add an item to the inventory.
    // Returns the quantity of items that could not be added (e.g., if inventory is full).
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    int32 AddItem(UItemDataAssetBase* ItemToAdd, int32 Quantity);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void UseItem(int32 SlotIndex);
    
    // The main function to remove a quantity of a specific item.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RemoveItem(UItemDataAssetBase* ItemToRemove, int32 Quantity);
    
    // Checks if the inventory contains at least a certain quantity of an item.
    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool HasItem(UItemDataAssetBase* ItemToFind, int32 Quantity = 1) const;

    // The actual list of items in the inventory.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
    TArray<FInventorySlot> Items;
    
    // The delegate that is broadcasted whenever the inventory contents change.
    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryUpdated OnInventoryUpdated;

protected:
    virtual void BeginPlay() override;

};