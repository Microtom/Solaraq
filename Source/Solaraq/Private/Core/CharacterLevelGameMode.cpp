// CharacterLevelGameMode.cpp
#include "Core/CharacterLevelGameMode.h"    // Adjust path
#include "Pawns/SolaraqCharacterPawn.h" // Adjust path
//#include "Controllers/SolaraqPlayerController.h" // Adjust path
#include "Core/SolaraqGameInstance.h"       // Adjust path
#include "Controllers/SolaraqCharacterPlayerController.h" 
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h" // For TActorIterator
#include "Controllers/SolaraqPlayerController.h"
#include "Logging/SolaraqLogChannels.h"


ACharacterLevelGameMode::ACharacterLevelGameMode()
{
    bUseSeamlessTravel = true;
    
    // Set the C++ default for the PlayerControllerClass member inherited from AGameModeBase
    PlayerControllerClass = ASolaraqCharacterPlayerController::StaticClass();

    // Set the C++ default for the DefaultPawnClass member inherited from AGameModeBase
    DefaultPawnClass = ASolaraqCharacterPawn::StaticClass();

    UE_LOG(LogSolaraqTransition, Warning, TEXT("ACharacterLevelGameMode C++ CONSTRUCTOR: PlayerControllerClass (base) set to %s, DefaultPawnClass (base) set to %s"),
        *GetNameSafe(PlayerControllerClass), *GetNameSafe(DefaultPawnClass));
}


void ACharacterLevelGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    USolaraqGameInstance* GI = GetGameInstance<USolaraqGameInstance>();
    if (GI)
    {
        // Changed GI->DockingPadIdentifier to GI->PlayerStartTagForCharacterLevel
        UE_LOG(LogSolaraqSystem, Log, TEXT("CharacterLevelGameMode: InitGame. Transitioning from Pad ID (now PlayerStartTag): %s (Origin: %s)"),
            *GI->PlayerStartTagForCharacterLevel.ToString(), *GI->OriginLevelName.ToString());
        // Here you could use GI->PlayerStartTagForCharacterLevel to select a specific PlayerStart
    }
}

void ACharacterLevelGameMode::RestartPlayer(AController* NewPlayer)
{
    Super::RestartPlayer(NewPlayer); // IMPORTANT: Call the base class version first.
                                     // This will handle finding a player start (using your FindPlayerStart_Implementation),
                                     // spawning the DefaultPawnClass (ASolaraqCharacterPawn),
                                     // and calling Possess on the NewPlayer.
                                     // ASolaraqPlayerController::OnPossess will be triggered during this Super call.

    // Your custom logic now runs *after* the base class has done its job.
    ASolaraqCharacterPlayerController* SolaraqPC = Cast<ASolaraqCharacterPlayerController>(NewPlayer);
    if (SolaraqPC)
    {
        // At this point, the pawn should be possessed.
        // The ASolaraqPlayerController's OnPossess method should have already
        // set the control mode and applied the correct input mapping context.
        // This is an additional check or place for GameMode-specific logic after possession.
        if (SolaraqPC->GetPawn() && Cast<ASolaraqCharacterPawn>(SolaraqPC->GetPawn()))
        {
             // Re-applying the context here acts as a safeguard or ensures it if
             // the OnPossess logic had any conditional paths that might have missed it
             // during complex level loads, though OnPossess is the primary place.
             
             UE_LOG(LogSolaraqSystem, Log, TEXT("CharacterLevelGameMode: Custom logic in RestartPlayer executed for %s. Pawn: %s. Input context (re)applied."), *GetNameSafe(SolaraqPC), *GetNameSafe(SolaraqPC->GetPawn()));
        }
        else
        {
            UE_LOG(LogSolaraqSystem, Warning, TEXT("CharacterLevelGameMode: In RestartPlayer, %s does not have a SolaraqCharacterPawn possessed after Super call."), *GetNameSafe(SolaraqPC));
        }
    }
     else
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("CharacterLevelGameMode: In RestartPlayer, NewPlayer (%s) is not a SolaraqPlayerController."), *GetNameSafe(NewPlayer));
    }
}

AActor* ACharacterLevelGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
    USolaraqGameInstance* GI = GetGameInstance<USolaraqGameInstance>();
    // Use the corrected variable name here:
    if (GI && !GI->PlayerStartTagForCharacterLevel.IsNone())
    {
        // Try to find a PlayerStart tagged with the PlayerStartTagForCharacterLevel
        for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
        {
            APlayerStart* PlayerStart = *It;
            if (PlayerStart && PlayerStart->PlayerStartTag == GI->PlayerStartTagForCharacterLevel) // Use corrected name
            {
                UE_LOG(LogSolaraqSystem, Log, TEXT("CharacterLevelGameMode: Found tagged PlayerStart '%s' for Pad ID '%s'."),
                    *PlayerStart->GetName(), *GI->PlayerStartTagForCharacterLevel.ToString()); // Use corrected name
                return PlayerStart;
            }
        }
        UE_LOG(LogSolaraqSystem, Warning, TEXT("CharacterLevelGameMode: No PlayerStart found with tag '%s'. Using default."), *GI->PlayerStartTagForCharacterLevel.ToString()); // Use corrected name
    }
    else if (GI)
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("CharacterLevelGameMode: GI valid, but PlayerStartTagForCharacterLevel is None. Using default PlayerStart."));
    }
    else
    {
         UE_LOG(LogSolaraqSystem, Warning, TEXT("CharacterLevelGameMode: GI is NULL. Using default PlayerStart."));
    }

    // Fallback to default PlayerStart finding logic
    return Super::FindPlayerStart_Implementation(Player, IncomingName);
}