// DestructibleAsteroid.h
#pragma once

#include "CoreMinimal.h"
#include "Environment/DestructibleBase.h"
#include "DestructibleAsteroid.generated.h"

// Forward Declaration
class UItemDataAssetBase;
class AItemPickup;

UCLASS()
class SOLARAQ_API ADestructibleAsteroid : public ADestructibleBase
{
	GENERATED_BODY()

public:
	ADestructibleAsteroid();

protected:
	virtual void OnFullyDestroyed_Implementation(AActor* DamageCauser) override;
	virtual void BeginPlay() override;

public:
	// CHANGED: Now we hold a list of Data Assets (e.g., Iron Data, Gold Data)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid|Loot")
	TArray<TObjectPtr<UItemDataAssetBase>> PossibleLootItems;

	// NEW: The generic Pickup Blueprint to spawn (e.g., BP_ItemPickup)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid|Loot")
	TSubclassOf<AItemPickup> LootPickupClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid|Loot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LootDropChance;
};