// SolaraqPlayerController.cpp

#include "Controllers/SolaraqPlayerController.h"
#include "EnhancedInputComponent.h" // Include for UEnhancedInputComponent
#include "EnhancedInputSubsystems.h" // Include for Subsystem
#include "InputMappingContext.h" // Include for UInputMappingContext
#include "InputAction.h" // Include for UInputAction
#include "Pawns/SolaraqShipBase.h" // Include ship base class
#include "GameFramework/Pawn.h"
#include "Logging/SolaraqLogChannels.h"
#include "GenericTeamAgentInterface.h" // Include if needed


ASolaraqPlayerController::ASolaraqPlayerController()
{
    // Constructor
    TeamId = FGenericTeamId(0); // Ensure Player Team ID is set
}

void ASolaraqPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // Get the Enhanced Input subsystem
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            // Add the mapping context
            if (DefaultMappingContext)
            {
                InputSubsystem->AddMappingContext(DefaultMappingContext, 0); // Priority 0
                UE_LOG(LogSolaraqSystem, Log, TEXT("Added Input Mapping Context %s"), *DefaultMappingContext->GetName());
            }
            else
            {
                 UE_LOG(LogSolaraqSystem, Error, TEXT("%s DefaultMappingContext is not assigned! Input will not work."), *GetName());
            }
        }
         else { UE_LOG(LogSolaraqSystem, Error, TEXT("Failed to get EnhancedInputLocalPlayerSubsystem!")); }
    }
     else { UE_LOG(LogSolaraqSystem, Error, TEXT("Failed to get LocalPlayer!")); }
}

void ASolaraqPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Cast InputComponent to UEnhancedInputComponent
    EnhancedInputComponentRef = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInputComponentRef)
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("%s Failed to cast InputComponent to UEnhancedInputComponent! Enhanced Input bindings will fail."), *GetName());
        return;
    }

     UE_LOG(LogSolaraqSystem, Log, TEXT("Setting up Enhanced Input Bindings for %s"), *GetName());

    // --- Bind Actions ---
    // Verify Input Action assets are assigned before binding
    if (MoveAction)
    {
        EnhancedInputComponentRef->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleMoveInput);
         UE_LOG(LogSolaraqSystem, Verbose, TEXT(" - MoveAction Bound"));
    } else { UE_LOG(LogSolaraqSystem, Warning, TEXT("MoveAction not assigned in PlayerController!")); }

    if (TurnAction)
    {
        EnhancedInputComponentRef->BindAction(TurnAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleTurnInput);
        EnhancedInputComponentRef->BindAction(TurnAction, ETriggerEvent::Completed, this, &ASolaraqPlayerController::HandleTurnCompleted);
        UE_LOG(LogSolaraqSystem, Verbose, TEXT(" - TurnAction Bound"));
    } else { UE_LOG(LogSolaraqSystem, Warning, TEXT("TurnAction not assigned in PlayerController!")); }

    if (FireAction)
    {
        // Bind to Triggered for single shots or hold-to-fire start
        EnhancedInputComponentRef->BindAction(FireAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleFireRequest);
         UE_LOG(LogSolaraqSystem, Verbose, TEXT(" - FireAction Bound"));
    } else { UE_LOG(LogSolaraqSystem, Warning, TEXT("FireAction not assigned in PlayerController!")); }

    if (BoostAction)
    {
        EnhancedInputComponentRef->BindAction(BoostAction, ETriggerEvent::Started, this, &ASolaraqPlayerController::HandleBoostStarted);
        EnhancedInputComponentRef->BindAction(BoostAction, ETriggerEvent::Completed, this, &ASolaraqPlayerController::HandleBoostCompleted);
         UE_LOG(LogSolaraqSystem, Verbose, TEXT(" - BoostAction Bound (Started/Completed)"));
    } else { UE_LOG(LogSolaraqSystem, Warning, TEXT("BoostAction not assigned in PlayerController!")); }
    
}

ASolaraqShipBase* ASolaraqPlayerController::GetControlledShip() const
{
    // Return cached pointer if valid, otherwise try to get and cast fresh
    // Note: This caching might need updating in OnPossess/OnUnPossess if the pawn changes frequently
    // For simplicity, we can just get it fresh each time input is handled.
    return Cast<ASolaraqShipBase>(GetPawn());
}


void ASolaraqPlayerController::HandleTurnCompleted(const FInputActionValue& Value)
{
    // Value passed might contain last value, but we know input stopped, so send 0.
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        // Explicitly send 0.0 turn input via the Server RPC
        Ship->Server_SendTurnInput(0.0f);
        // UE_LOG(LogSolaraqInput, Verbose, TEXT("HandleTurnCompleted: Sending 0.0 Turn Input"));

        // Optional: Update local pawn immediately if needed for prediction,
        // but Method 1 relies on server replication anyway.
        // Ship->SetTurnInputForRoll(0.0f);
    }
}

void ASolaraqPlayerController::HandleMoveInput(const FInputActionValue& Value)
{
    // Input is typically Vector2D (X=Strafe/Swizzle, Y=Forward/Backward)
    // Adjust based on your IA_Move configuration (e.g., if it's just 1D float for forward)
    const float MoveValue = Value.Get<float>(); // Assuming 1D Axis for forward/backward

    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        // Call the Server RPC on the Pawn
        Ship->Server_SendMoveForwardInput(MoveValue);
         // UE_LOG(LogSolaraqInput, Verbose, TEXT("HandleMoveInput: %.2f"), MoveValue);
    }
}

void ASolaraqPlayerController::HandleTurnInput(const FInputActionValue& Value)
{
    // Input is typically Float (Yaw Rate)
    const float TurnValue = Value.Get<float>();

    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        // Call the Server RPC on the Pawn
        Ship->Server_SendTurnInput(TurnValue);
        // UE_LOG(LogSolaraqInput, Verbose, TEXT("HandleTurnInput: %.2f"), TurnValue);
    }
}

void ASolaraqPlayerController::HandleFireRequest() // Changed parameter list
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        // Call the Server RPC on the Pawn
        Ship->Server_RequestFire();
        // UE_LOG(LogSolaraqInput, Verbose, TEXT("HandleFireRequest called"));
    }
}

void ASolaraqPlayerController::HandleBoostStarted(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        // Call the Server RPC on the Pawn
        Ship->Server_SetAttemptingBoost(true);
         // UE_LOG(LogSolaraqInput, Verbose, TEXT("HandleBoostStarted"));
    }
}

void ASolaraqPlayerController::HandleBoostCompleted(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        // Call the Server RPC on the Pawn
        Ship->Server_SetAttemptingBoost(false);
        // UE_LOG(LogSolaraqInput, Verbose, TEXT("HandleBoostCompleted"));
    }
}

// --- Optional: Implement GetTeamAttitudeTowards if PlayerController handles team ---
/*
ETeamAttitude::Type ASolaraqPlayerController::GetTeamAttitudeTowards(const AActor& Other) const
{
    // ... (Implementation similar to previous examples) ...
}
*/