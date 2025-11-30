// InventoryComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

// Forward declaration
class UItemDataAssetBase;

USTRUCT(BlueprintType)
struct FPlacedItem
{
    GENERATED_BODY()

    // The item data asset.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UItemDataAssetBase> ItemData = nullptr;

    // How many of this item are in this stack.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Quantity = 0;

    // The top-left corner coordinate of this item in the inventory grid.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint TopLeft = FIntPoint(0, 0);

    // A unique ID to make finding and moving this specific item easier.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid ItemID;

    FPlacedItem()
    {
        ItemID = FGuid::NewGuid();
    }

    FPlacedItem(UItemDataAssetBase* InData, int32 InQuantity, FIntPoint InTopLeft)
        : ItemData(InData), Quantity(InQuantity), TopLeft(InTopLeft)
    {
        ItemID = FGuid::NewGuid();
    }

    bool IsValid() const { return ItemData != nullptr && Quantity > 0; }

    bool operator==(const FPlacedItem& Other) const
    {
        return this->ItemID == Other.ItemID;
    }
};

// Represents a single slot in the inventory. It links an item type (DataAsset) with a quantity.
USTRUCT(BlueprintType)
struct FInventorySlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Inventory")
    TObjectPtr<UItemDataAssetBase> ItemData = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Inventory", meta = (ClampMin = "0"))
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
    UFUNCTION(BlueprintCallable, Category = "Solaraq|Inventory")
    int32 AddItem(UItemDataAssetBase* ItemToAdd, int32 Quantity);

    UFUNCTION(BlueprintCallable, Category = "Solaraq|Inventory")
    bool AddItemAt(const FPlacedItem& Item, FIntPoint TopLeft);
    
    UFUNCTION(BlueprintCallable, Category = "Solaraq|Inventory")
    void UseItem(const FGuid& ItemID);
    
    // The main function to remove a quantity of a specific item.
    UFUNCTION(BlueprintCallable, Category = "Solaraq|Inventory")
    void RemoveItem(const FGuid& ItemID, int32 QuantityToRemove);
    
    // Checks if the inventory contains at least a certain quantity of an item.
    UFUNCTION(BlueprintPure, Category = "Solaraq|Inventory")
    bool HasItem(UItemDataAssetBase* ItemToFind, int32 Quantity = 1) const;

    // The actual list of items in the inventory.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Inventory")
    TArray<FInventorySlot> Items;
    
    // The delegate that is broadcasted whenever the inventory contents change.
    UPROPERTY(BlueprintAssignable, Category = "Solaraq|Inventory")
    FOnInventoryUpdated OnInventoryUpdated;

    UFUNCTION(BlueprintCallable, Category="Solaraq|Inventory")
    bool MoveItem(const FGuid& ItemID, FIntPoint NewTopLeft);

    /** Checks if an item can be moved to a new location without actually moving it. */
    bool CanMoveItemTo(const FGuid& ItemID, FIntPoint NewTopLeft);

    /** Moves an item from this inventory to a Target Inventory Component. */
    UFUNCTION(BlueprintCallable, Category = "Solaraq|Inventory")
    bool TransferItemTo(UInventoryComponent* TargetInventory, const FGuid& ItemID, FIntPoint TargetPos);
    
    /** Returns the configured width of the inventory grid. */
    UFUNCTION(BlueprintPure, Category = "Solaraq|Inventory|Grid")
    int32 GetGridWidth() const;

    /** Returns the configured height of the inventory grid. */
    UFUNCTION(BlueprintPure, Category = "Solaraq|Inventory|Grid")
    int32 GetGridHeight() const;

    /** Returns a constant reference to the array of all placed items. */
    const TArray<FPlacedItem>& GetPlacedItems() const;
    
protected:
    virtual void BeginPlay() override;

private:
    // The list of all items currently placed in the inventory.
    UPROPERTY(VisibleAnywhere, Category = "Solaraq|Inventory")
    TArray<FPlacedItem> PlacedItems;

    // The total size of the inventory grid.
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Inventory")
    int32 InventoryWidth = 8;
    
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Inventory")
    int32 InventoryHeight = 10;

    // Helper function to check if a specific area is available.
    bool IsAreaFree(FIntPoint TopLeft, FIntPoint Dimensions) const;

    // Helper function to find the first available spot for an item of a given size.
    bool FindFreeSpot(FIntPoint Dimensions, FIntPoint& OutTopLeft) const;

};

