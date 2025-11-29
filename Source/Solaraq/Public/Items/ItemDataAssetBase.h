// ItemDataAsset.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemDataAssetBase.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EEquipmentSlot : uint8
{
	None        UMETA(DisplayName = "None"),
	Head        UMETA(DisplayName = "Head"),
	Body        UMETA(DisplayName = "Body"),
	MainHand    UMETA(DisplayName = "Main Hand"),
	OffHand     UMETA(DisplayName = "Off Hand"),
	// Add Feet, Hands, Ring, etc. as needed
};

UENUM(BlueprintType)
enum class EItemType : uint8
{
	Resource UMETA(DisplayName = "Resource"),
	ShipModule UMETA(DisplayName = "Ship Module"),
	Consumable UMETA(DisplayName = "Consumable"),
	QuestItem UMETA(DisplayName = "Quest Item"),
	Tool UMETA(DisplayName = "Tool"),         
	Weapon UMETA(DisplayName = "Weapon"), 
	Generic UMETA(DisplayName = "Generic")
};

/**
 * The BASE DataAsset for all items. It contains only properties shared by ALL items.
 */
UCLASS(BlueprintType)
class SOLARAQ_API UItemDataAssetBase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// The type of item, used for quick filtering and logic branching.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Item")
	EItemType ItemType;
    
	// The name displayed in the UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Item")
	FText DisplayName;

	// The description displayed in the UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Item", meta = (MultiLine = true))
	FText Description;

	// The icon to display in the UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Item")
	TObjectPtr<UTexture2D> Icon;

	// The static mesh to use when this item is dropped in the world
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Item")
	TObjectPtr<UStaticMesh> PickupMesh;

	// Can this item be stacked in the inventory?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Stacking")
	bool bIsStackable = true;

	// The maximum number of items in a single stack. Ignored if bIsStackable is false.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Stacking", meta = (EditCondition = "bIsStackable", ClampMin = "1"))
	int32 MaxStackSize = 100;

	// The dimensions of the item in inventory slots (Width, Height).
	// A 1x1 item is the default. A sword might be 1x3, a helmet 2x2.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Inventory")
	FIntPoint Dimensions = FIntPoint(1, 1);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Item")
	EEquipmentSlot EquipmentSlot = EEquipmentSlot::None;
};