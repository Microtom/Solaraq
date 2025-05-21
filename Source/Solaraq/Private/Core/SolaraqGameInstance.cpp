// SolaraqGameInstance.cpp
#include "Core/SolaraqGameInstance.h" // Adjust path as needed
#include "Logging/SolaraqLogChannels.h"

USolaraqGameInstance::USolaraqGameInstance()
{
	TargetLevelToLoad = NAME_None;
	OriginLevelName = NAME_None;
	DockingPadIdentifier = NAME_None;
}

void USolaraqGameInstance::PrepareForCharacterLevelLoad(FName InTargetLevel, const FTransform& InShipTransform, FName InOriginLevel, FName InDockingPadID)
{
	TargetLevelToLoad = InTargetLevel;
	ShipTransformInOriginLevel = InShipTransform;
	OriginLevelName = InOriginLevel;
	DockingPadIdentifier = InDockingPadID;
	UE_LOG(LogSolaraqSystem, Log, TEXT("GameInstance: Preparing for Character Level Load. Target: %s, Origin: %s, PadID: %s"), 
		*TargetLevelToLoad.ToString(), *OriginLevelName.ToString(), *DockingPadIdentifier.ToString());
}

void USolaraqGameInstance::PrepareForShipLevelLoad(FName InTargetLevel, FName InOriginLevel)
{
	TargetLevelToLoad = InTargetLevel;
	OriginLevelName = InOriginLevel;
	// ShipTransformInOriginLevel is already set from the previous transition or initial game start
	// DockingPadIdentifier might not be relevant when returning to ship level directly.
	UE_LOG(LogSolaraqSystem, Log, TEXT("GameInstance: Preparing for Ship Level Load. Target: %s, Origin: %s"),
		*TargetLevelToLoad.ToString(), *OriginLevelName.ToString());
}


void USolaraqGameInstance::ClearTransitionData()
{
	// Optionally clear data after it has been used by the GameMode of the new level
	// TargetLevelToLoad = NAME_None; 
	// OriginLevelName = NAME_None;
	// DockingPadIdentifier = NAME_None; 
	// For now, let's not clear automatically, GameMode can do it.
	UE_LOG(LogSolaraqSystem, Log, TEXT("GameInstance: Transition data potentially cleared (manual call needed)."));
}