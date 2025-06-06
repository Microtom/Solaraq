// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/ItemDataAssetBase.h"
#include "ItemConsumableDataAsset.generated.h"

/**
 * 
 */
UCLASS()
class SOLARAQ_API UItemConsumableDataAsset : public UItemDataAssetBase
{
	GENERATED_BODY()
public:
	UItemConsumableDataAsset()
	{
		// Set the default type for any item created from this class
		ItemType = EItemType::Consumable;
	}

	// The amount of health to restore to the character when used.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	float HealthToRestore = 25.0f;

	// The amount of shield/hull to restore to the ship when used.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	float ShipHealthToRestore = 100.0f;

	// The sound to play when the item is used.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	TObjectPtr<USoundBase> UseSound;
};
