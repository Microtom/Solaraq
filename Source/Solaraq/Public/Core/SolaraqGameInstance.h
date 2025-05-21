// SolaraqGameInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SolaraqGameInstance.generated.h"

UCLASS()
class SOLARAQ_API USolaraqGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	USolaraqGameInstance();

	// Information about the transition
	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	FName TargetLevelToLoad; // Name of the level to load (e.g., "CharacterTestLevel")

	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	FName OriginLevelName; // Name of the level we came from (e.g., your space level)
    
	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	FTransform ShipTransformInOriginLevel; // Where the ship was in the space level

	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	FName DockingPadIdentifier; // A way to identify which docking pad was used, if needed for specific spawn points in the new level.

	// Called when transitioning from Ship to Character
	UFUNCTION(BlueprintCallable, Category = "Transition")
	void PrepareForCharacterLevelLoad(FName InTargetLevel, const FTransform& InShipTransform, FName InOriginLevel, FName InDockingPadID = NAME_None);

	// Called when transitioning from Character to Ship
	UFUNCTION(BlueprintCallable, Category = "Transition")
	void PrepareForShipLevelLoad(FName InTargetLevel, FName InOriginLevel);


	// Helper to clear transition data
	UFUNCTION(BlueprintCallable, Category = "Transition")
	void ClearTransitionData();
};