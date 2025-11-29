// ItemArmorDataAsset.h
#pragma once

#include "CoreMinimal.h"
#include "Items/ItemDataAssetBase.h"
#include "ItemArmorDataAsset.generated.h"

/**
 * Data Asset specifically for clothing and armor (Head, Body, etc.)
 */
UCLASS()
class SOLARAQ_API UItemArmorDataAsset : public UItemDataAssetBase
{
	GENERATED_BODY()

public:
	// You can add armor specific stats here later
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solaraq|Armor Stats")
	float DefensePower = 10.0f;
};