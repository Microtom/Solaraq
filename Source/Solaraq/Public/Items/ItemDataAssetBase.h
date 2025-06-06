// ItemDataAsset.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemDataAssetBase.generated.h"

class UTexture2D;

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
 * Do not create item instances directly from this; use a more specific child class.
 */
UCLASS(BlueprintType, Abstract) // Note: Marked as Abstract!
class SOLARAQ_API UItemDataAssetBase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// The type of item, used for quick filtering and logic branching.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EItemType ItemType;
    
	// The name displayed in the UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	// The description displayed in the UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText Description;

	// The icon to display in the UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon;

	// The static mesh to use when this item is dropped in the world
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMesh> PickupMesh;

	// Can this item be stacked in the inventory?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stacking")
	bool bIsStackable = true;

	// The maximum number of items in a single stack. Ignored if bIsStackable is false.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stacking", meta = (EditCondition = "bIsStackable", ClampMin = "1"))
	int32 MaxStackSize = 100;
};