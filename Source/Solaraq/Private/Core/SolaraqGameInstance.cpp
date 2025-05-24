// SolaraqGameInstance.cpp
#include "Core/SolaraqGameInstance.h" // Adjust path as needed
#include "Logging/SolaraqLogChannels.h"

USolaraqGameInstance::USolaraqGameInstance()
{
	TargetLevelToLoad = NAME_None;
	OriginLevelName = NAME_None;
	PlayerStartTagForCharacterLevel = NAME_None; // Renamed from DockingPadIdentifier
	DockingPadIdentifierToReturnTo = NAME_None;
	PlayerShipNameInOriginLevel = NAME_None;
	ShipDockedRelativeRotation = FRotator::ZeroRotator;
}

void USolaraqGameInstance::PrepareForCharacterLevelLoad(FName InTargetLevel, const FTransform& InShipTransform, FName InOriginLevel, FName InPlayerStartTag, FName InDockingPadIDToReturnTo, FName InPlayerShipName)
{
	TargetLevelToLoad = InTargetLevel;                  // Character Level Name
	ShipTransformInOriginLevel = InShipTransform;       // Ship's world transform in Space Level
	OriginLevelName = InOriginLevel;                    // Space Level Name (came from)
	PlayerStartTagForCharacterLevel = InPlayerStartTag; // For CharacterLevelGameMode PlayerStart
	DockingPadIdentifierToReturnTo = InDockingPadIDToReturnTo; // DockingPadUniqueID for finding ship later
	PlayerShipNameInOriginLevel = InPlayerShipName;     // Ship's FName for finding ship later

	UE_LOG(LogSolaraqSystem, Log, TEXT("GameInstance: Preparing for Character Level Load. Target: %s, Origin: %s, PlayerStartTag: %s, ReturnPadID: %s, ShipName: %s"),
		*TargetLevelToLoad.ToString(),
		*OriginLevelName.ToString(),
		*PlayerStartTagForCharacterLevel.ToString(),
		*DockingPadIdentifierToReturnTo.ToString(),
		*PlayerShipNameInOriginLevel.ToString());
}

void USolaraqGameInstance::PrepareForShipLevelLoad(FName InTargetShipLevel, FName InCurrentCharacterLevel)
{
	TargetLevelToLoad = InTargetShipLevel;       // The space level we are returning to.
	OriginLevelName = InCurrentCharacterLevel; // The character level we are coming from.
	// PlayerShipNameInOriginLevel and DockingPadIdentifierToReturnTo should still be populated from the initial Ship->Char transition.
	
	UE_LOG(LogSolaraqSystem, Log, TEXT("GameInstance: Preparing for Ship Level Load. Target: %s (Space Level), Origin: %s (Char Level)"),
		*TargetLevelToLoad.ToString(), *OriginLevelName.ToString());
	UE_LOG(LogSolaraqSystem, Log, TEXT("  Relevant data for re-possessing ship: ShipName='%s', PadID='%s'"),
		*PlayerShipNameInOriginLevel.ToString(), *DockingPadIdentifierToReturnTo.ToString());
}


void USolaraqGameInstance::ClearTransitionData()
{
	UE_LOG(LogSolaraqSystem, Log, TEXT("GameInstance: Clearing ALL transition data."));
	TargetLevelToLoad = NAME_None;
	OriginLevelName = NAME_None;
	PlayerStartTagForCharacterLevel = NAME_None;
	DockingPadIdentifierToReturnTo = NAME_None;
	PlayerShipNameInOriginLevel = NAME_None;
	ShipTransformInOriginLevel = FTransform::Identity;
	ShipDockedRelativeRotation = FRotator::ZeroRotator;
	// Consider more granular clearing if needed, e.g., ClearDataAfterCharacterLoad, ClearDataAfterShipReturn
}