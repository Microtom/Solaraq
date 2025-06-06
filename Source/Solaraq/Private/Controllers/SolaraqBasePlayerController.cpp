// SolaraqBasePlayerController.cpp

#include "Controllers/SolaraqBasePlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Core/SolaraqGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/SolaraqLogChannels.h"
#include "Components/DockingPadComponent.h"
#include "Pawns/SolaraqCharacterPawn.h" // Included for completeness if base ever needs to know about it
#include "Pawns/SolaraqShipBase.h"       // Included for GI preparation

ASolaraqBasePlayerController::ASolaraqBasePlayerController()
{
    // TeamId is initialized in the header.
    EnhancedInputComponentRef = nullptr;
}

void ASolaraqBasePlayerController::BeginPlay()
{
    Super::BeginPlay();
    // Derived classes handle applying their specific input contexts here or in OnPossess.
}

void ASolaraqBasePlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    // Derived classes will cast InPawn to their specific type and may set up input contexts.
}

void ASolaraqBasePlayerController::OnUnPossess()
{
    Super::OnUnPossess();
    // Derived classes might perform specific cleanup here.
}

void ASolaraqBasePlayerController::OnRep_Pawn()
{
    Super::OnRep_Pawn();
    // Called on clients when the pawn they control changes.
    // Derived controllers should re-apply their input context here if necessary.
}

void ASolaraqBasePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    EnhancedInputComponentRef = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInputComponentRef)
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("ASolaraqBasePlayerController (%s): Failed to cast InputComponent to UEnhancedInputComponent! Enhanced Input bindings will fail in derived classes."), *GetName());
        return;
    }
    // Actual action bindings are performed in derived classes.
}

void ASolaraqBasePlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    // Derived classes implement their specific Tick logic.
}

USolaraqGameInstance* ASolaraqBasePlayerController::GetSolaraqGameInstance() const
{
    return GetGameInstance<USolaraqGameInstance>();
}

// This function is typically called by a pawn (e.g., ASolaraqShipBase) when interaction for transition is requested.
// It should make an RPC to the server version of this PlayerController.
void ASolaraqBasePlayerController::RequestCharacterLevelTransition(FName TargetLevelName, FName DockingPadID, ASolaraqShipBase* FromShip)
{
    // This client-side function should now make a server RPC
    // to request the server to initiate the travel for this client.
    // Example: Server_Reliable_RequestCharacterTravel(TargetLevelName, DockingPadID, FromShip);
    // The implementation of that Server RPC would then call:
    // Server_InitiateSeamlessTravelToLevel(TargetLevelName, true, DockingPadID, FromShip);

    UE_LOG(LogSolaraqTransition, Warning, TEXT("BasePC %s: RequestCharacterLevelTransition called. This should ideally trigger a Server RPC that then calls Server_InitiateSeamlessTravelToLevel."), *GetNameSafe(this));
    // For now, as a placeholder until Server RPC is in derived class or ship:
    if (IsLocalPlayerController()) // To avoid issues if called on server directly by mistake
    {
        Server_InitiateSeamlessTravelToLevel(TargetLevelName, true, DockingPadID);
    }
}

// Similar to RequestCharacterLevelTransition, this should trigger a server RPC.
void ASolaraqBasePlayerController::RequestShipLevelTransition(FName TargetShipLevelName)
{
    UE_LOG(LogSolaraqTransition, Warning, TEXT("BasePC %s: RequestShipLevelTransition called. This should ideally trigger a Server RPC that then calls Server_InitiateSeamlessTravelToLevel."), *GetNameSafe(this));
    // For now, as a placeholder:
    if (IsLocalPlayerController()) // To avoid issues if called on server directly by mistake
    {
         Server_InitiateSeamlessTravelToLevel(TargetShipLevelName, false);
    }
}

void ASolaraqBasePlayerController::Server_InitiateSeamlessTravelToLevel(FName TargetLevelName, bool bIsCharacterLevel, FName PlayerStartOrPadID)
{
    // This function MUST run on the server.
    if (!HasAuthority())
    {
        // If a client calls this, it needs to be an RPC to the server version.
        // For simplicity in this refactor, we'll assume the call path ensures this is the server.
        // A proper implementation would have a separate UFUNCTION(Server, Reliable) that clients call,
        // which then calls this authoritative function.
        UE_LOG(LogSolaraqTransition, Error, TEXT("BasePC %s: Server_InitiateSeamlessTravelToLevel called by non-authority! This is incorrect. Client should RPC to server."), *GetNameSafe(this));
        return;
    }
    FString TravelURL = TargetLevelName.ToString();
    
    UE_LOG(LogSolaraqTransition, Log, TEXT("BasePC %s (SERVER): EXECUTING a generic ClientTravel request to URL: '%s'."),
        *GetNameSafe(this), *TravelURL);
        
    // This is now the ONLY thing this function does. It is clean of any AActor* references.
    ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute, true /*bSeamless*/, FGuid());
}

void ASolaraqBasePlayerController::HostGame()
{
    USolaraqGameInstance* GI = GetGameInstance<USolaraqGameInstance>();
    if (GI)
    {
        GI->HostSession();
    }
}

void ASolaraqBasePlayerController::JoinGame()
{
    USolaraqGameInstance* GI = GetGameInstance<USolaraqGameInstance>();
    if (GI)
    {
        GI->FindAndJoinSession();
    }
}


void ASolaraqBasePlayerController::ClearAllInputContexts(UEnhancedInputLocalPlayerSubsystem* InputSubsystem)
{
    if (InputSubsystem)
    {
        InputSubsystem->ClearAllMappings();
        UE_LOG(LogSolaraqSystem, Verbose, TEXT("ASolaraqBasePlayerController (%s): Cleared all input contexts."), *GetName());
    }
}

void ASolaraqBasePlayerController::AddInputContext(UEnhancedInputLocalPlayerSubsystem* InputSubsystem, UInputMappingContext* ContextToAdd, int32 Priority)
{
    if (InputSubsystem && ContextToAdd)
    {
        InputSubsystem->AddMappingContext(ContextToAdd, Priority);
        // UE_LOG(LogSolaraqSystem, Verbose, TEXT("ASolaraqBasePlayerController (%s): Added Input Mapping Context: %s with priority %d"), *GetName(), *ContextToAdd->GetName(), Priority);
    }
    else
    {
        if (!InputSubsystem) UE_LOG(LogSolaraqSystem, Error, TEXT("ASolaraqBasePlayerController (%s): AddInputContext failed - InputSubsystem is null."), *GetName());
        if (!ContextToAdd) UE_LOG(LogSolaraqSystem, Error, TEXT("ASolaraqBasePlayerController (%s): AddInputContext failed - ContextToAdd is null."), *GetName());
    }
}