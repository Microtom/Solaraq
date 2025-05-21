// CharacterLevelGameMode.cpp
#include "Core/CharacterLevelGameMode.h"    // Adjust path
#include "Pawns/SolaraqCharacterPawn.h" // Adjust path
#include "Controllers/SolaraqPlayerController.h" // Adjust path
#include "Core/SolaraqGameInstance.h"       // Adjust path
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h" // For TActorIterator
#include "Logging/SolaraqLogChannels.h"


ACharacterLevelGameMode::ACharacterLevelGameMode()
{
    DefaultPawnClass = ASolaraqCharacterPawn::StaticClass();

    // Path derived from the Blueprint reference you provided
    static const TCHAR* PlayerControllerBPPath = TEXT("/Game/Blueprints/Controllers/BP_SolaraqPlayerController.BP_SolaraqPlayerController_C");
    static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerBPClass(PlayerControllerBPPath);

    if (PlayerControllerBPClass.Succeeded()) // Succeeded() checks if .Class is not null
    {
        PlayerControllerClass = PlayerControllerBPClass.Class;
        UE_LOG(LogSolaraqTransition, Warning, TEXT("CharacterLevelGameMode Constructor: Successfully set PlayerControllerClass to the class found at: %s"), PlayerControllerBPPath);
    }
    else
    {
        UE_LOG(LogSolaraqTransition, Warning, TEXT("CharacterLevelGameMode Constructor: Could not find PlayerController Blueprint class at path: %s. Falling back to C++ ASolaraqPlayerController. INPUTS MIGHT BE MISSING."), PlayerControllerBPPath);
        PlayerControllerClass = ASolaraqPlayerController::StaticClass(); // Fallback
    }
}

void ACharacterLevelGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    USolaraqGameInstance* GI = GetGameInstance<USolaraqGameInstance>();
    if (GI)
    {
        UE_LOG(LogSolaraqSystem, Log, TEXT("CharacterLevelGameMode: InitGame. Transitioning from Pad ID: %s (Origin: %s)"), 
            *GI->DockingPadIdentifier.ToString(), *GI->OriginLevelName.ToString());
        // Here you could use GI->DockingPadIdentifier to select a specific PlayerStart
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
    ASolaraqPlayerController* SolaraqPC = Cast<ASolaraqPlayerController>(NewPlayer);
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
             SolaraqPC->ApplyInputContextForCurrentMode();
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
    if (GI && !GI->DockingPadIdentifier.IsNone())
    {
        // Try to find a PlayerStart tagged with the DockingPadIdentifier
        for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
        {
            APlayerStart* PlayerStart = *It;
            if (PlayerStart && PlayerStart->PlayerStartTag == GI->DockingPadIdentifier)
            {
                UE_LOG(LogSolaraqSystem, Log, TEXT("CharacterLevelGameMode: Found tagged PlayerStart '%s' for Pad ID '%s'."), 
                    *PlayerStart->GetName(), *GI->DockingPadIdentifier.ToString());
                return PlayerStart;
            }
        }
        UE_LOG(LogSolaraqSystem, Warning, TEXT("CharacterLevelGameMode: No PlayerStart found with tag '%s'. Using default."), *GI->DockingPadIdentifier.ToString());
    }
    
    // Fallback to default PlayerStart finding logic
    return Super::FindPlayerStart_Implementation(Player, IncomingName);
}