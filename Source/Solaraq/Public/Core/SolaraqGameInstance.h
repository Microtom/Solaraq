// SolaraqGameInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h" 
#include "SolaraqGameInstance.generated.h"

UCLASS()
class SOLARAQ_API USolaraqGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	USolaraqGameInstance();

	virtual void Init() override;
	
	// Call this to host a new session
	UFUNCTION(BlueprintCallable, Category="Solaraq|Network")
	void HostSession();

	// Call this to find and join an available session
	UFUNCTION(BlueprintCallable, Category="Solaraq|Network")
	void FindAndJoinSession();

	// Information about the transition
	UPROPERTY(BlueprintReadWrite, Category = "Solaraq|Transition")
	FName TargetLevelToLoad; // Name of the level to load

	UPROPERTY(BlueprintReadWrite, Category = "Solaraq|Transition")
	FName OriginLevelName; // Name of the level we came from

	UPROPERTY(BlueprintReadWrite, Category = "Solaraq|Transition")
	FTransform ShipTransformInOriginLevel; // Where the ship was in the space level (world transform)

	// Used by CharacterLevelGameMode to find a specific PlayerStart based on the docking pad used.
	UPROPERTY(BlueprintReadWrite, Category = "Solaraq|Transition")
	FName PlayerStartTagForCharacterLevel; 

	UPROPERTY(BlueprintReadWrite, Category = "Solaraq|Transition")
	FRotator ShipDockedRelativeRotation;
	
	// Used by SolaraqSpaceLevelGameMode to find the specific ship and verify its docking location.
	UPROPERTY(BlueprintReadWrite, Category = "Solaraq|Transition")
	FName DockingPadIdentifierToReturnTo; // The DockingPadUniqueID of the pad the ship should be at.

	UPROPERTY(BlueprintReadWrite, Category = "Solaraq|Transition")
	FName PlayerShipNameInOriginLevel; // The FName of the player's ship actor in the origin space level.


	// Called when transitioning from Ship to Character
	UFUNCTION(BlueprintCallable, Category = "Solaraq|Transition")
	void PrepareForCharacterLevelLoad(FName InTargetLevel, const FTransform& InShipTransform, FName InOriginLevel, FName InPlayerStartTag, FName InDockingPadIDToReturnTo, FName InPlayerShipName); // Added new params

	// Called when transitioning from Character to Ship
	UFUNCTION(BlueprintCallable, Category = "Solaraq|Transition")
	void PrepareForShipLevelLoad(FName InTargetShipLevel, FName InCurrentCharacterLevel);


	// Helper to clear transition data
	UFUNCTION(BlueprintCallable, Category = "Solaraq|Transition")
	void ClearTransitionData();
	

	/**
	 * Prepares the GameInstance with data needed when transitioning FROM a Ship Level TO a Character Level.
	 * Called by the server's PlayerController before initiating client travel.
	 * @param InTargetCharacterLevel The name of the character level to load.
	 * @param InShipTransformInOrigin The transform of the ship in the origin (space) level.
	 * @param InOriginSpaceLevel The name of the origin (space) level.
	 * @param InPlayerStartTagForCharLevel The tag for the APlayerStart in the character level (usually DockingPadUniqueID).
	 * @param InDockingPadIDToReturnTo The UniqueID of the docking pad to return to in the space level.
	 * @param InPlayerShipName The FName of the player's ship actor in the space level.
	 * @param InShipDockedRelRotation The relative rotation of the ship to its docking pad.
	 */
	void PrepareForCharacterLevelLoad(
		FName InTargetCharacterLevel,
		const FTransform& InShipTransformInOrigin,
		FName InOriginSpaceLevel,
		FName InPlayerStartTagForCharLevel,
		FName InDockingPadIDToReturnTo,
		FName InPlayerShipName,
		FRotator InShipDockedRelRotation
	);

protected:
	// The session interface, our gateway to all session functionality
	IOnlineSessionPtr SessionInterface;

	// Delegate called when a session is created
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	// Delegate called when sessions are found
	void OnFindSessionsComplete(bool bWasSuccessful);
	// Delegate called when a session is joined
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	// Shared pointer to our session search results
	TSharedPtr<class FOnlineSessionSearch> SessionSearch;
};