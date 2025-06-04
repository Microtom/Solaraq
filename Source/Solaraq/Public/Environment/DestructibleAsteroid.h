#pragma once

#include "CoreMinimal.h"
#include "Environment/DestructibleBase.h" // Ensure correct path
#include "DestructibleAsteroid.generated.h"

UCLASS()
class SOLARAQ_API ADestructibleAsteroid : public ADestructibleBase
{
	GENERATED_BODY()

public:
	ADestructibleAsteroid();

protected:
	//~ Begin ADestructibleBase Interface
	// Override if asteroids have unique full destruction behavior beyond base effects/logic
	virtual void OnFullyDestroyed_Implementation(AActor* DamageCauser) override;
	// Override if asteroids have unique piece breaking behavior
	// virtual void OnPieceBroken_Implementation(const FVector& PieceLocation, const FVector& PieceImpulseDir) override;
	//~ End ADestructibleBase Interface

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	//~ End AActor Interface

public:
	// Asteroid-specific properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroid|Loot")
	TArray<TSubclassOf<class AActor>> PossibleLootDrops;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroid|Loot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LootDropChance;

	// You could add more asteroid-specific things, like type of material, size category, etc.
};