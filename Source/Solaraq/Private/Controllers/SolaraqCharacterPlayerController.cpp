// SolaraqCharacterPlayerController.cpp

#include "Controllers/SolaraqCharacterPlayerController.h" // Adjust to your path
#include "Pawns/SolaraqCharacterPawn.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Core/SolaraqGameInstance.h" // For level transition
#include "Kismet/GameplayStatics.h" // For OpenLevel
#include "Logging/SolaraqLogChannels.h"

ASolaraqCharacterPlayerController::ASolaraqCharacterPlayerController()
{
    // Constructor
}

ASolaraqCharacterPawn* ASolaraqCharacterPlayerController::GetControlledCharacter() const
{
    return Cast<ASolaraqCharacterPawn>(GetPawn());
}

void ASolaraqCharacterPlayerController::ApplyCharacterInputMappingContext()
{
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            ClearAllInputContexts(InputSubsystem); // Call base helper

            if (IMC_CharacterControls)
            {
                AddInputContext(InputSubsystem, IMC_CharacterControls, 0); // Call base helper
                UE_LOG(LogSolaraqSystem, Log, TEXT("ASolaraqCharacterPlayerController: Applied CHARACTER Input Mapping Context: %s"), *IMC_CharacterControls->GetName());
            }
            else
            {
                UE_LOG(LogSolaraqSystem, Error, TEXT("ASolaraqCharacterPlayerController: IMC_CharacterControls is not assigned! Character input will not work."));
            }
        }
    }
}

void ASolaraqCharacterPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (GetPawn()) // Only apply if we already possess a pawn
    {
        ApplyCharacterInputMappingContext();
    }
}

void ASolaraqCharacterPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    ASolaraqCharacterPawn* PossessedChar = Cast<ASolaraqCharacterPawn>(InPawn);
    FString AuthorityPrefix = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");

    if (PossessedChar)
    {
        UE_LOG(LogSolaraqMovement, Warning, TEXT("%s ASolaraqCharacterPlayerController (%s): OnPossess - Possessing CHARACTER: %s"),
            *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(PossessedChar));
        ApplyCharacterInputMappingContext();
    }
    else
    {
        UE_LOG(LogSolaraqMovement, Error, TEXT("%s ASolaraqCharacterPlayerController (%s): OnPossess - FAILED to cast InPawn (%s) to ASolaraqCharacterPawn."),
            *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(InPawn));
        if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                ClearAllInputContexts(InputSubsystem);
            }
        }
    }
}

void ASolaraqCharacterPlayerController::OnUnPossess()
{
    FString AuthorityPrefix = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
    APawn* UnpossessedPawn = GetPawn(); // Get pawn before Super::OnUnPossess clears it internally
    UE_LOG(LogSolaraqMovement, Log, TEXT("%s ASolaraqCharacterPlayerController (%s): OnUnPossess - Unpossessing: %s."),
        *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(UnpossessedPawn));
    
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if (IMC_CharacterControls)
            {
                InputSubsystem->RemoveMappingContext(IMC_CharacterControls);
            }
        }
    }
    Super::OnUnPossess();
}

void ASolaraqCharacterPlayerController::OnRep_Pawn()
{
    Super::OnRep_Pawn();
    ApplyCharacterInputMappingContext(); // Re-apply context on client if pawn changes
}

void ASolaraqCharacterPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent(); // Gets EnhancedInputComponentRef

    if (!EnhancedInputComponentRef)
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("ASolaraqCharacterPlayerController: EnhancedInputComponentRef is NULL after Super::SetupInputComponent! Cannot bind character actions."));
        return;
    }
    
    UE_LOG(LogSolaraqSystem, Log, TEXT("ASolaraqCharacterPlayerController: Setting up CHARACTER Input Bindings for %s"), *GetName());

    if (CharacterMoveAction) EnhancedInputComponentRef->BindAction(CharacterMoveAction, ETriggerEvent::Triggered, this, &ASolaraqCharacterPlayerController::HandleCharacterMoveInput);
    // Bind other character actions (Look, Jump) here if you add them

    // InteractAction is defined in ASolaraqBasePlayerController
    // Bind it here for character-specific interaction
    if (InteractAction) { 
        EnhancedInputComponentRef->BindAction(InteractAction, ETriggerEvent::Started, this, &ASolaraqCharacterPlayerController::HandleCharacterInteractInput);
        UE_LOG(LogSolaraqTransition, Warning, TEXT("ASolaraqCharacterPlayerController %s: SetupInputComponent - SUCCESSFULLY BOUND InteractAction to HandleCharacterInteractInput."), *GetNameSafe(this));
    } else {
        UE_LOG(LogSolaraqTransition, Error, TEXT("ASolaraqCharacterPlayerController %s: SetupInputComponent - InteractAction IS NULL! Cannot bind HandleCharacterInteractInput."), *GetNameSafe(this));
    }
    if (PrimaryUseAction)
    {
        EnhancedInputComponentRef->BindAction(PrimaryUseAction, ETriggerEvent::Started, this, &ASolaraqCharacterPlayerController::HandlePrimaryUseStarted);
        EnhancedInputComponentRef->BindAction(PrimaryUseAction, ETriggerEvent::Completed, this, &ASolaraqCharacterPlayerController::HandlePrimaryUseCompleted);
    }
    if (SecondaryUseAction)
    {
        EnhancedInputComponentRef->BindAction(SecondaryUseAction, ETriggerEvent::Started, this, &ASolaraqCharacterPlayerController::HandleSecondaryUseStarted);
        EnhancedInputComponentRef->BindAction(SecondaryUseAction, ETriggerEvent::Completed, this, &ASolaraqCharacterPlayerController::HandleSecondaryUseCompleted);
    }
}

void ASolaraqCharacterPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    // Character-specific tick logic, if any (e.g., managing a character-specific UI element)
}

void ASolaraqCharacterPlayerController::HandleCharacterMoveInput(const FInputActionValue& Value)
{
    ASolaraqCharacterPawn* CharPawn = GetControlledCharacter();
    if (CharPawn)
    {
        const FVector2D MovementVector = Value.Get<FVector2D>();
        CharPawn->HandleMoveInput(MovementVector);
    }
}

void ASolaraqCharacterPlayerController::HandleCharacterInteractInput()
{
    UE_LOG(LogSolaraqTransition, Warning, TEXT("CharacterPC %s: HandleCharacterInteractInput called."), *GetNameSafe(this));
    ASolaraqCharacterPawn* CharPawn = GetControlledCharacter();
    if (CharPawn)
    {
        UE_LOG(LogSolaraqTransition, Warning, TEXT("CharacterPC %s: GetControlledCharacter() returned: %s. Attempting to return to ship level."), *GetNameSafe(this), *GetNameSafe(CharPawn));
        USolaraqGameInstance* GI = GetSolaraqGameInstance(); // Use base class helper
        if (GI)
        {
            FName SpaceLevelToReturnTo = GI->OriginLevelName;
            if (SpaceLevelToReturnTo != NAME_None)
            {
                UE_LOG(LogSolaraqTransition, Warning, TEXT("CharacterPC %s: GI->OriginLevelName is valid. Calling InitiateLevelTransitionToShip with '%s'."), *GetNameSafe(this), *SpaceLevelToReturnTo.ToString());
                
                // Before calling the transition, this controller needs to handle unpossessing and destroying its pawn
                if (IsLocalController()) // Only client should destroy its pawn this way before travel
                {
                    UnPossess(); // This will call OnUnPossess to remove input context
                    if (CharPawn->IsPendingKillPending() == false)
                    {
                         CharPawn->Destroy();
                    }
                }
                // Server will handle pawn destruction via normal game flow or replication if needed.
                // Or, GameMode could clean up pawns on level change.
                // For seamless travel, destroying pawn before calling OpenLevel is common for client.

                Super::RequestShipLevelTransition(SpaceLevelToReturnTo); // Call base class method
            }
            else
            {
                UE_LOG(LogSolaraqTransition, Error, TEXT("CharacterPC %s: Cannot transition to ship. GameInstance OriginLevelName is not set."), *GetNameSafe(this));
            }
        }
        else
        {
            UE_LOG(LogSolaraqTransition, Error, TEXT("CharacterPC %s: GetSolaraqGameInstance() returned NULL."), *GetNameSafe(this));
        }
    }
    else
    {
        UE_LOG(LogSolaraqTransition, Error, TEXT("CharacterPC %s: GetControlledCharacter() returned NULL."), *GetNameSafe(this));
    }
}

void ASolaraqCharacterPlayerController::HandlePrimaryUseStarted()
{
    if (ASolaraqCharacterPawn* CharPawn = GetControlledCharacter())
    {
        if (UEquipmentComponent* EquipComp = CharPawn->GetEquipmentComponent())
        {
            EquipComp->HandlePrimaryUse(); // Pass the command to the pawn's component
        }
    }
}

void ASolaraqCharacterPlayerController::HandlePrimaryUseCompleted()
{
    if (ASolaraqCharacterPawn* CharPawn = GetControlledCharacter())
    {
        if (UEquipmentComponent* EquipComp = CharPawn->GetEquipmentComponent())
        {
            EquipComp->HandlePrimaryUse_Stop(); // Pass the command to the pawn's component
        }
    }
}

void ASolaraqCharacterPlayerController::HandleSecondaryUseStarted()
{
    if (ASolaraqCharacterPawn* CharPawn = GetControlledCharacter())
    {
        if (UEquipmentComponent* EquipComp = CharPawn->GetEquipmentComponent())
        {
            EquipComp->HandleSecondaryUse(); // Pass the command to the pawn's component
        }
    }
}

void ASolaraqCharacterPlayerController::HandleSecondaryUseCompleted()
{
    if (ASolaraqCharacterPawn* CharPawn = GetControlledCharacter())
    {
        if (UEquipmentComponent* EquipComp = CharPawn->GetEquipmentComponent())
        {
            EquipComp->HandleSecondaryUse_Stop(); // Pass the command to the pawn's component
        }
    }
}