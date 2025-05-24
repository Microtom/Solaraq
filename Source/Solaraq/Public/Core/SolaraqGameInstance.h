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
	FName TargetLevelToLoad; // Name of the level to load

	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	FName OriginLevelName; // Name of the level we came from

	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	FTransform ShipTransformInOriginLevel; // Where the ship was in the space level (world transform)

	// Used by CharacterLevelGameMode to find a specific PlayerStart based on the docking pad used.
	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	FName PlayerStartTagForCharacterLevel; 

	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	FRotator ShipDockedRelativeRotation;
	
	// Used by SolaraqSpaceLevelGameMode to find the specific ship and verify its docking location.
	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	FName DockingPadIdentifierToReturnTo; // The DockingPadUniqueID of the pad the ship should be at.

	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	FName PlayerShipNameInOriginLevel; // The FName of the player's ship actor in the origin space level.


	// Called when transitioning from Ship to Character
	UFUNCTION(BlueprintCallable, Category = "Transition")
	void PrepareForCharacterLevelLoad(FName InTargetLevel, const FTransform& InShipTransform, FName InOriginLevel, FName InPlayerStartTag, FName InDockingPadIDToReturnTo, FName InPlayerShipName); // Added new params

	// Called when transitioning from Character to Ship
	UFUNCTION(BlueprintCallable, Category = "Transition")
	void PrepareForShipLevelLoad(FName InTargetShipLevel, FName InCurrentCharacterLevel);


	// Helper to clear transition data
	UFUNCTION(BlueprintCallable, Category = "Transition")
	void ClearTransitionData();

    // UPROPERTY(BlueprintReadOnly, Category = "Transition") // These seem unused for now, can be removed or kept if planned for future
    // FName OwningActorOfDockingPadTag; 

    // UPROPERTY(BlueprintReadOnly, Category = "Transition") // This is covered by DockingPadIdentifierToReturnTo
    // FName DockingPadIdentifierToReturnTo; 

    // UPROPERTY(BlueprintReadOnly, Category = "Transition")
    // FTransform ShipRelativeTransformToPad;
};