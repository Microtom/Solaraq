// SolaraqGameInstance.cpp
#include "Core/SolaraqGameInstance.h" // Adjust path as needed

#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Logging/SolaraqLogChannels.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"

USolaraqGameInstance::USolaraqGameInstance()
{
	TargetLevelToLoad = NAME_None;
	OriginLevelName = NAME_None;
	PlayerStartTagForCharacterLevel = NAME_None; // Renamed from DockingPadIdentifier
	DockingPadIdentifierToReturnTo = NAME_None;
	PlayerShipNameInOriginLevel = NAME_None;
	ShipDockedRelativeRotation = FRotator::ZeroRotator;
}

void USolaraqGameInstance::Init()
{
	Super::Init();

	// Get the Online Subsystem
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		UE_LOG(LogSolaraqSystem, Warning, TEXT("Found Online Subsystem: %s"), *Subsystem->GetSubsystemName().ToString());
		SessionInterface = Subsystem->GetSessionInterface();
	}
	else
	{
		UE_LOG(LogSolaraqSystem, Error, TEXT("Could not find any Online Subsystem."));
	}
}

void USolaraqGameInstance::HostSession()
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogSolaraqSystem, Error, TEXT("Cannot host session, Session Interface is not valid."));
		return;
	}

	// Bind our delegate for when session creation is complete
	SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &USolaraqGameInstance::OnCreateSessionComplete);

	// Set up the session settings
	FOnlineSessionSettings SessionSettings;
	SessionSettings.bIsLANMatch = true; // Use true for Null subsystem
	SessionSettings.NumPublicConnections = 4;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bUsesPresence = true;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.Set(FName("GAMETYPE"), FString("Solaraq_FreeForAll"), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	
	UE_LOG(LogSolaraqSystem, Log, TEXT("Attempting to create session..."));
	SessionInterface->CreateSession(0, FName("My Solaraq Session"), SessionSettings);
}

void USolaraqGameInstance::FindAndJoinSession()
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogSolaraqSystem, Error, TEXT("Cannot find session, Session Interface is not valid."));
		return;
	}

	// Bind our delegate for when finding sessions is complete
	SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &USolaraqGameInstance::OnFindSessionsComplete);

	// Create and configure a new search object
	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->bIsLanQuery = true;
	SessionSearch->MaxSearchResults = 100;
	SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

	UE_LOG(LogSolaraqSystem, Log, TEXT("Searching for sessions..."));
	SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
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

void USolaraqGameInstance::PrepareForCharacterLevelLoad(FName InTargetCharacterLevel,
	const FTransform& InShipTransformInOrigin, FName InOriginSpaceLevel, FName InPlayerStartTagForCharLevel,
	FName InDockingPadIDToReturnTo, FName InPlayerShipName, FRotator InShipDockedRelRotation)
{
	TargetLevelToLoad = InTargetCharacterLevel;
	ShipTransformInOriginLevel = InShipTransformInOrigin;
	OriginLevelName = InOriginSpaceLevel;
	PlayerStartTagForCharacterLevel = InPlayerStartTagForCharLevel;
	DockingPadIdentifierToReturnTo = InDockingPadIDToReturnTo;
	PlayerShipNameInOriginLevel = InPlayerShipName;
	ShipDockedRelativeRotation = InShipDockedRelRotation;
}

void USolaraqGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogSolaraqSystem, Log, TEXT("OnCreateSessionComplete: Success = %d"), bWasSuccessful);

	if (bWasSuccessful)
	{
		// Travel to the lobby map as a Listen Server
		UE_LOG(LogSolaraqSystem, Log, TEXT("Session created successfully. Traveling to the Space Level as a listen server..."));
		GetWorld()->ServerTravel("/Game/Maps/SpaceStationTest?listen");
	}
}

void USolaraqGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
	UE_LOG(LogSolaraqSystem, Log, TEXT("OnFindSessionsComplete: Success = %d"), bWasSuccessful);

	if (bWasSuccessful && SessionSearch.IsValid() && SessionSearch->SearchResults.Num() > 0)
	{
		UE_LOG(LogSolaraqSystem, Log, TEXT("Found %d sessions. Joining the first one."), SessionSearch->SearchResults.Num());

		// Bind the join session delegate
		SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &USolaraqGameInstance::OnJoinSessionComplete);
		
		// Join the first search result
		SessionInterface->JoinSession(0, FName("My Solaraq Session"), SessionSearch->SearchResults[0]);
	}
	else
	{
		UE_LOG(LogSolaraqSystem, Warning, TEXT("Could not find any sessions."));
	}
}

void USolaraqGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		UE_LOG(LogSolaraqSystem, Log, TEXT("Successfully joined session: %s"), *SessionName.ToString());
		
		// Get the connect string and travel
		FString ConnectString;
		if (SessionInterface->GetResolvedConnectString(SessionName, ConnectString))
		{
			APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
			if (PC)
			{
				UE_LOG(LogSolaraqSystem, Log, TEXT("Traveling to server at: %s"), *ConnectString);
				PC->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
			}
		}
	}
	else
	{
		UE_LOG(LogSolaraqSystem, Error, TEXT("Failed to join session. Result: %d"), (int32)Result);
	}
}
