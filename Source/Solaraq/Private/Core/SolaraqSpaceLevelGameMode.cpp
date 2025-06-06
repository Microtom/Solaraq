// SolaraqSpaceLevelGameMode.cpp
#include "Core/SolaraqSpaceLevelGameMode.h"

#include "EngineUtils.h"
#include "Pawns/SolaraqShipBase.h"
#include "Controllers/SolaraqShipPlayerController.h"
#include "Core/SolaraqGameInstance.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Logging/SolaraqLogChannels.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerState.h" 

ASolaraqSpaceLevelGameMode::ASolaraqSpaceLevelGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    
    bUseSeamlessTravel = true;
    
    // Set the C++ default for the PlayerControllerClass member inherited from AGameModeBase
    PlayerControllerClass = ASolaraqShipPlayerController::StaticClass();

    // Set the C++ default for the DefaultPawnClass member inherited from AGameModeBase
    // If PlayerShipClassToSpawn is set in a Blueprint derived from this,
    // GetDefaultPawnClassForController_Implementation will prioritize it.
    // Otherwise, this DefaultPawnClass will be used.
    DefaultPawnClass = ASolaraqShipBase::StaticClass(); // Fallback C++ default

    // Note: If PlayerShipClassToSpawn is a UPROPERTY, its value from a Blueprint Game Mode
    // will be available when GetDefaultPawnClassForController_Implementation is called.
    // So, we don't need to check it directly in the constructor for DefaultPawnClass here.
    // The GetDefaultPawnClassForController_Implementation override will handle the priority.

    UE_LOG(LogSolaraqSystem, Warning, TEXT("ASolaraqSpaceLevelGameMode C++ CONSTRUCTOR: PlayerControllerClass (base) set to %s, DefaultPawnClass (base) set to %s"),
        *GetNameSafe(PlayerControllerClass), *GetNameSafe(DefaultPawnClass));
}

void ASolaraqSpaceLevelGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (GEngine)
    {
        FString DebugString = FString::Printf(TEXT("SERVER PLAYER STATUS (World: %s)"), *GetWorld()->GetName());

        // The GameState holds the list of all connected players.
        if (GameState && GameState->PlayerArray.Num() > 0)
        {
            // Iterate over all PlayerStates in the game.
            for (APlayerState* PlayerState : GameState->PlayerArray)
            {
                if (PlayerState)
                {
                    // Get the PlayerController from the PlayerState.
                    APlayerController* PC = PlayerState->GetPlayerController();
                    FString PlayerName = PlayerState->GetPlayerName();
                    
                    if (PC)
                    {
                        // Get the Pawn the controller is currently possessing.
                        APawn* CurrentPawn = PC->GetPawn();
                        FString PawnName = GetNameSafe(CurrentPawn);
                        
                        // Get the Level the PlayerController is currently in. THIS IS THE KEY.
                        ULevel* PlayerLevel = PC->GetLevel();
                        FString LevelName = GetNameSafe(PlayerLevel);

                        DebugString += FString::Printf(TEXT("\n - Player: %s | Level: %s | Pawn: %s"), *PlayerName, *LevelName, *PawnName);
                    }
                    else
                    {
                        // This can happen briefly during transitions.
                        DebugString += FString::Printf(TEXT("\n - Player: %s | (No PlayerController)"), *PlayerName);
                    }
                }
            }
        }
        else
        {
            DebugString += "\n(No players in GameState)";
        }

        GEngine->AddOnScreenDebugMessage(12345, 0.0f, FColor::Cyan, DebugString, false, FVector2D(1.5f, 1.5f));
    }

    // --- Throttled UE_LOG message (every LogInterval seconds) ---
    TimeSinceLastLog += DeltaSeconds;
    if (TimeSinceLastLog >= LogInterval)
    {
        TimeSinceLastLog = 0.0f; // Reset the timer

        FString LogString = FString::Printf(TEXT("SERVER WORLD STATE REPORT (GameMode: %s, World: %s)"), *GetName(), *GetWorld()->GetName());
        if (GameState && GameState->PlayerArray.Num() > 0)
        {
            for (APlayerState* PlayerState : GameState->PlayerArray)
            {
                if (PlayerState)
                {
                    APlayerController* PC = PlayerState->GetPlayerController();
                    FString PlayerName = PlayerState->GetPlayerName();
                    
                    if (PC)
                    {
                        APawn* CurrentPawn = PC->GetPawn();
                        FString PawnName = GetNameSafe(CurrentPawn);
                        ULevel* PlayerLevel = PC->GetLevel();
                        FString LevelName = GetNameSafe(PlayerLevel);

                        LogString += FString::Printf(TEXT("\n\t> Player: [%s] is in Level: [%s] controlling Pawn: [%s]"), 
                            *PlayerName, *LevelName, *PawnName);
                    }
                    else
                    {
                        LogString += FString::Printf(TEXT("\n\t> Player: [%s] has no valid PlayerController."), *PlayerName);
                    }
                }
            }
        }
        else
        {
            LogString += "\n\t(No players in GameState)";
        }

        // Log to the "LogSolaraqTransition" category for easy filtering.
        UE_LOG(LogSolaraqTransition, Warning, TEXT("%s"), *LogString);
    }
}

// Let Super handle these for now to mimic AGameModeBase behavior
AActor* ASolaraqSpaceLevelGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
    
    UE_LOG(LogSolaraqSystem, Log, TEXT("ASolaraqSpaceLevelGameMode (Restored FindPlayerStart): Not a transition with GI data, or data invalid. Calling Super::FindPlayerStart_Implementation for %s."), *GetNameSafe(Player));
    return Super::FindPlayerStart_Implementation(Player, IncomingName);
    
}



UClass* ASolaraqSpaceLevelGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    // Prioritize your custom UPROPERTY if it's set in the Blueprint Game Mode
    if (PlayerShipClassToSpawn)
    {
        UE_LOG(LogSolaraqSystem, Log, TEXT("ASolaraqSpaceLevelGameMode GetDefaultPawnClass: Using PlayerShipClassToSpawn UPROPERTY: %s"), *PlayerShipClassToSpawn->GetName());
        return PlayerShipClassToSpawn;
    }
    
    // Otherwise, fall back to the DefaultPawnClass member of AGameModeBase (which we set a C++ default for).
    // The Blueprint Game Mode can also directly override this AGameModeBase::DefaultPawnClass.
    UE_LOG(LogSolaraqSystem, Log, TEXT("ASolaraqSpaceLevelGameMode GetDefaultPawnClass: PlayerShipClassToSpawn not set. Using AGameModeBase::DefaultPawnClass: %s"), *GetNameSafe(Super::GetDefaultPawnClassForController_Implementation(InController)));
    return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ASolaraqSpaceLevelGameMode::RestartPlayer(AController* NewPlayer)
{
    UE_LOG(LogSolaraqSystem, Log, TEXT("ASolaraqSpaceLevelGameMode::RestartPlayer for %s. Current World: %s"), *GetNameSafe(NewPlayer), *GetWorld()->GetName());
    USolaraqGameInstance* GI = GetGameInstance<USolaraqGameInstance>();
    ASolaraqShipBase* ShipToPossess = nullptr; // Renamed from FoundShip

    // ... (Pre-Search Logging from previous step) ...

    bool bFoundAndPossessedOriginalShip = false;

    if (GI && !GI->PlayerShipNameInOriginLevel.IsNone() && !GI->DockingPadIdentifierToReturnTo.IsNone())
    {
        FString TargetPadIDString = GI->DockingPadIdentifierToReturnTo.IsNone() ? FString(TEXT("ANY (IsNone was true, should not happen with outer if)")) : GI->DockingPadIdentifierToReturnTo.ToString();
        UE_LOG(LogSolaraqSystem, Log, TEXT("  Attempting to find and possess original ship '%s' expected at pad ID '%s'"),
            *GI->PlayerShipNameInOriginLevel.ToString(),
            *TargetPadIDString);

        for (TActorIterator<ASolaraqShipBase> It(GetWorld()); It; ++It)
        {
            ASolaraqShipBase* PotentialShip = *It;
            // Using FName for PlayerShipNameInOriginLevel
            if (PotentialShip && PotentialShip->GetFName() == GI->PlayerShipNameInOriginLevel)
            {
                ShipToPossess = PotentialShip; // Found the ship by name
                UE_LOG(LogSolaraqSystem, Log, TEXT("    Found original ship by FName: %s."), *ShipToPossess->GetName());
                break;
            }
        }

        if (ShipToPossess)
        {
            APawn* CurrentPawn = NewPlayer->GetPawn();
            if (CurrentPawn && CurrentPawn != ShipToPossess) { CurrentPawn->Destroy(); }
            
            NewPlayer->Possess(ShipToPossess);
            bFoundAndPossessedOriginalShip = true;
            UE_LOG(LogSolaraqSystem, Log, TEXT("  Successfully re-possessed original ship: %s"), *ShipToPossess->GetName());
        }
        else
        {
             UE_LOG(LogSolaraqSystem, Warning, TEXT("  WARNING: Did not find original ship by name '%s'. Will spawn new ship."), *GI->PlayerShipNameInOriginLevel.ToString());
        }
    }
    else
    {
        // ... (logging for why GI data is insufficient) ...
    }

    if (!bFoundAndPossessedOriginalShip)
    {
        UE_LOG(LogSolaraqSystem, Log, TEXT("  Did not re-possess original ship. Calling Super::RestartPlayer to spawn a new ship."));
        Super::RestartPlayer(NewPlayer); // Spawns a new pawn and possesses it
    }

    // ---- AFTER POSSESSION (either original or new) ----
    ASolaraqShipBase* PossessedPlayerShip = Cast<ASolaraqShipBase>(NewPlayer->GetPawn());
    if (PossessedPlayerShip)
    {
        UE_LOG(LogSolaraqSystem, Log, TEXT("  Player %s now possesses ship %s. Attempting re-establish docking if applicable."), *NewPlayer->GetName(), *PossessedPlayerShip->GetName());
        // This call needs to be on the server instance of the ship
        if (PossessedPlayerShip->HasAuthority()) // Should be true since GameMode runs on server
        {
            PossessedPlayerShip->Server_AttemptReestablishDockingAfterLoad();
        }
        else
        {
            // This case should be rare for GameMode logic, but good to be aware of.
            // Might need an RPC if the GameMode somehow ended up with a client ship reference.
            UE_LOG(LogSolaraqSystem, Error, TEXT("  Possessed ship %s for player %s does not have authority. Cannot call Server_AttemptReestablishDockingAfterLoad directly."),
                *PossessedPlayerShip->GetName(), *NewPlayer->GetName());
        }
    }
    else
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("  Player %s does not possess a SolaraqShipBase after RestartPlayer logic. Pawn is: %s"),
            *NewPlayer->GetName(), *GetNameSafe(NewPlayer->GetPawn()));
    }
}

// Keep these new ones from previous debugging for now:
void ASolaraqSpaceLevelGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    UE_LOG(LogSolaraqSystem, Warning, TEXT("ASolaraqSpaceLevelGameMode (Simplified)::InitGame - Map: %s, GameMode THIS: %s, Class: %s"), 
        *MapName, *GetNameSafe(this), *GetClass()->GetName());
}

bool ASolaraqSpaceLevelGameMode::PlayerCanRestart(APlayerController* Player)
{
    bool bOriginalCanRestart = Super::PlayerCanRestart(Player);
    UE_LOG(LogSolaraqSystem, Warning, TEXT("ASolaraqSpaceLevelGameMode (Simplified)::PlayerCanRestart for Player: %s. Super returned: %d."), 
        *GetNameSafe(Player), bOriginalCanRestart);
    return bOriginalCanRestart;
}