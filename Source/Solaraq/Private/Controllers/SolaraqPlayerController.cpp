// SolaraqPlayerController.cpp

#include "Controllers/SolaraqPlayerController.h" // Ensure this matches your path

#include "EngineUtils.h"
#include "Pawns/SolaraqShipBase.h"             // Ensure this matches your path
#include "Pawns/SolaraqCharacterPawn.h"        // << NEW: Include CharacterPawn
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Logging/SolaraqLogChannels.h" // Your custom log channels
#include "Blueprint/UserWidget.h"
#include "Core/SolaraqGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "UI/TargetWidgetInterface.h"
// Other includes you might have (EngineUtils, etc.)

ASolaraqPlayerController::ASolaraqPlayerController()
{
    // TeamId is initialized in the header
    bIsHomingLockActive = false;
    LockedHomingTargetIndex = -1;
    HomingTargetScanRange = 25000.0f;
    HomingTargetScanConeAngleDegrees = 90.0f;
    HomingTargetScanInterval = 0.5f;

    // NEW: Initialize pawn control variables
    CurrentControlMode = EPlayerControlMode::Ship; // Default to ship control
    PossessedShipPawn = nullptr;
    PossessedCharacterPawn = nullptr;
}

void ASolaraqPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // Initial pawn possession is handled by the engine calling OnPossess.
    // We apply the input context based on the initially possessed pawn.
    ApplyInputContextForCurrentMode();
}

void ASolaraqPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    // Log on both server and client to compare, but especially important for client
    FString AuthorityPrefix = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
    UE_LOG(LogSolaraqMovement, Warning, TEXT("%s PC %s: OnPossess - Possessing: %s (Class: %s)"),
        *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(InPawn), *GetNameSafe(InPawn ? InPawn->GetClass() : nullptr));

    if (ASolaraqShipBase* Ship = Cast<ASolaraqShipBase>(InPawn))
    {
        PossessedShipPawn = Ship;
        PossessedCharacterPawn = nullptr; // Ensure character is cleared if we are re-possessing a ship
        CurrentControlMode = EPlayerControlMode::Ship;
        UE_LOG(LogSolaraqMovement, Warning, TEXT("%s PC %s: OnPossess successfully cast to ASolaraqShipBase. Mode set to Ship. PossessedShipPawn is now %s."),
            *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(PossessedShipPawn));
    }
    else if (ASolaraqCharacterPawn* PossessedChar = Cast<ASolaraqCharacterPawn>(InPawn))
    {
        PossessedCharacterPawn = PossessedChar;
        // PossessedShipPawn should remain valid if we came from it (it's "parked")
        CurrentControlMode = EPlayerControlMode::Character;
        UE_LOG(LogSolaraqMovement, Warning, TEXT("%s PC %s: OnPossess successfully cast to ASolaraqCharacterPawn. Mode set to Character. PossessedCharacterPawn is now %s. PossessedShipPawn remains %s."),
            *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(PossessedCharacterPawn), *GetNameSafe(PossessedShipPawn));
    }
    else
    {
        UE_LOG(LogSolaraqMovement, Error, TEXT("%s PC %s: OnPossess FAILED to cast InPawn (%s) to known type. Control mode may be incorrect."),
            *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(InPawn));
        CurrentControlMode = EPlayerControlMode::Ship; // Fallback, though this case should be rare
    }
    
    ApplyInputContextForCurrentMode();
}

void ASolaraqPlayerController::OnUnPossess()
{
    FString AuthorityPrefix = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
    UE_LOG(LogSolaraqMovement, Log, TEXT("%s PC %s: OnUnPossess - Unpossessing: %s. Clearing local possessed pawn refs."),
        *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(GetPawn())); // GetPawn() might be the old pawn here

    Super::OnUnPossess();
}

void ASolaraqPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    EnhancedInputComponentRef = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInputComponentRef)
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("%s Failed to cast InputComponent to UEnhancedInputComponent! Enhanced Input bindings will fail."), *GetName());
        return;
    }

    UE_LOG(LogSolaraqSystem, Log, TEXT("Setting up Enhanced Input Bindings for %s"), *GetName());

    // --- Bind Ship Actions (Existing) ---
    if (MoveAction) EnhancedInputComponentRef->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleMoveInput);
    if (TurnAction)
    {
        EnhancedInputComponentRef->BindAction(TurnAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleTurnInput);
        EnhancedInputComponentRef->BindAction(TurnAction, ETriggerEvent::Completed, this, &ASolaraqPlayerController::HandleTurnCompleted);
    }
    if (FireAction) EnhancedInputComponentRef->BindAction(FireAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleFireRequest);
    if (BoostAction)
    {
        EnhancedInputComponentRef->BindAction(BoostAction, ETriggerEvent::Started, this, &ASolaraqPlayerController::HandleBoostStarted);
        EnhancedInputComponentRef->BindAction(BoostAction, ETriggerEvent::Completed, this, &ASolaraqPlayerController::HandleBoostCompleted);
    }
    if (FireMissileAction) EnhancedInputComponentRef->BindAction(FireMissileAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleFireMissileRequest);
    if (ToggleLockAction) EnhancedInputComponentRef->BindAction(ToggleLockAction, ETriggerEvent::Started, this, &ASolaraqPlayerController::HandleToggleLock);
    if (SwitchTargetAction) EnhancedInputComponentRef->BindAction(SwitchTargetAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleSwitchTarget);

    if (InteractAction) { 
        EnhancedInputComponentRef->BindAction(InteractAction, ETriggerEvent::Started, this, &ASolaraqPlayerController::HandleInteractInput);
        UE_LOG(LogSolaraqTransition, Warning, TEXT("PC %s: SetupInputComponent - SUCCESSFULLY BOUND InteractAction to HandleInteractInput."), *GetNameSafe(this));
    } else {
        UE_LOG(LogSolaraqTransition, Error, TEXT("PC %s: SetupInputComponent - InteractAction IS NULL! Cannot bind HandleInteractInput."), *GetNameSafe(this));
    }
    
    // --- Bind Character Actions (NEW) ---
    if (CharacterMoveAction) EnhancedInputComponentRef->BindAction(CharacterMoveAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleCharacterMoveInput);

    // --- Bind Shared Actions (NEW) ---
    if (TogglePawnModeAction) EnhancedInputComponentRef->BindAction(TogglePawnModeAction, ETriggerEvent::Started, this, &ASolaraqPlayerController::HandleTogglePawnModeInput);

    // Apply initial context (also done in BeginPlay/OnPossess, but good to have a call here too)
    ApplyInputContextForCurrentMode();
}

void ASolaraqPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Update widget positions every frame if lock is active AND in ship mode
    if (CurrentControlMode == EPlayerControlMode::Ship && bIsHomingLockActive)
    {
        UpdateTargetWidgets();
    }
}

void ASolaraqPlayerController::OnRep_Pawn()
{
    FString AuthorityPrefix = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"); // Should always be CLIENT here
    APawn* PawnBeforeSuper = GetPawn();

    // Log before Super call
    // NET_LOG(LogSolaraqSystem, Log, TEXT("%s PC %s: OnRep_Pawn BEGIN. Pawn (from GetPawn() before Super call): %s."),
    //     *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(PawnBeforeSuper));

    Super::OnRep_Pawn(); // Let the base class do its work to update GetPawn() and potentially call OnPossess/OnUnPossess

    APawn* CurrentReplicatedPawn = GetPawn(); // This is the pawn the controller is now officially associated with

    // NET_LOG(LogSolaraqSystem, Log, TEXT("%s PC %s: OnRep_Pawn END after Super. Pawn (from GetPawn()): %s."),
    //     *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(CurrentReplicatedPawn));

    // Now, regardless of whether Super::OnRep_Pawn() managed to call our full OnPossess chain
    // to update PossessedShipPawn, we ensure our state is correct based on CurrentReplicatedPawn.
    // This is more robust because AController's OnPossess/OnUnPossess might have different conditions
    // for firing than just the pawn pointer changing.

    if (CurrentReplicatedPawn)
    {
        // If we are now controlling a pawn, explicitly run our OnPossess logic
        // This ensures PossessedShipPawn/PossessedCharacterPawn and CurrentControlMode are set.
        // Our OnPossess already checks if it's a ship or character.
        // This might re-call OnPossess if the engine also called it, but OnPossess should be idempotent
        // (safe to call multiple times with the same pawn).
        // Let's check if our internal state *actually needs* updating to avoid redundant calls if possible.

        bool bNeedsUpdate = false;
        if (ASolaraqShipBase* Ship = Cast<ASolaraqShipBase>(CurrentReplicatedPawn)) {
            if (PossessedShipPawn != Ship || CurrentControlMode != EPlayerControlMode::Ship) {
                bNeedsUpdate = true;
            }
        } else if (ASolaraqCharacterPawn* Char = Cast<ASolaraqCharacterPawn>(CurrentReplicatedPawn)) {
            if (PossessedCharacterPawn != Char || CurrentControlMode != EPlayerControlMode::Character) {
                bNeedsUpdate = true;
            }
        } else {
            // Replicated pawn is not a ship or character we manage this way
            // If we were previously possessing something, we might need to clear our state
            if (PossessedShipPawn != nullptr || PossessedCharacterPawn != nullptr) {
                 // This case implies an unpossess of our managed types, or possession of an unknown type.
                 // The engine should have called OnUnPossess. For safety, we can clear here.
                UE_LOG(LogSolaraqMovement, Warning, TEXT("%s PC %s: OnRep_Pawn - Replicated pawn %s is not managed ship/char. Clearing local possessed vars."),
                        *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(CurrentReplicatedPawn));
                PossessedShipPawn = nullptr;
                PossessedCharacterPawn = nullptr;
                // What should CurrentControlMode be? Perhaps keep it, or have an "Unknown" state.
                // For now, let OnPossess handle the mode if it's called with a valid type.
            }
        }

        if (bNeedsUpdate) {
            UE_LOG(LogSolaraqMovement, Warning, TEXT("%s PC %s: OnRep_Pawn - State needs update or re-sync. Calling OnPossess with %s."),
                    *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(CurrentReplicatedPawn));
            OnPossess(CurrentReplicatedPawn); // This will set PossessedShipPawn/Char and CurrentControlMode
        }
    }
    else // CurrentReplicatedPawn is nullptr (unpossessed)
    {
        // The engine should have called our OnUnPossess.
        // We ensure our internal state reflects this.
        if (PossessedShipPawn != nullptr || PossessedCharacterPawn != nullptr)
        {
            UE_LOG(LogSolaraqMovement, Warning, TEXT("%s PC %s: OnRep_Pawn - Replicated pawn is NULL. Calling OnUnPossess to clear local state."),
                    *AuthorityPrefix, *GetNameSafe(this));
            OnUnPossess(); // Call our OnUnPossess to clear PossessedShipPawn/Char
                           // Note: Our OnUnPossess doesn't currently clear these, it's mostly a log.
                           // It might be better to explicitly clear them here or in OnUnPossess.
            // For robustness:
            PossessedShipPawn = nullptr;
            PossessedCharacterPawn = nullptr;
            // CurrentControlMode = EPlayerControlMode::Ship; // Or some default "limbo" state
        }
    }
}

void ASolaraqPlayerController::ClearAllInputContexts()
{
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if (DefaultMappingContext) InputSubsystem->RemoveMappingContext(DefaultMappingContext); // Ship context
            if (IMC_CharacterControls) InputSubsystem->RemoveMappingContext(IMC_CharacterControls); // Character context
            //UE_LOG(LogSolaraqSystem, Log, TEXT("Cleared all known input contexts."));
        }
    }
}

void ASolaraqPlayerController::ApplyInputContextForCurrentMode()
{
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            ClearAllInputContexts(); // Clear existing contexts first

            if (CurrentControlMode == EPlayerControlMode::Ship)
            {
                if (DefaultMappingContext) // Your existing ship context
                {
                    InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
                    UE_LOG(LogSolaraqSystem, Log, TEXT("Applied SHIP Input Mapping Context: %s"), *DefaultMappingContext->GetName());
                }
                else UE_LOG(LogSolaraqSystem, Error, TEXT("ASolaraqPlayerController: DefaultMappingContext (for Ship) is not assigned! Input will not work."));
            }
            else if (CurrentControlMode == EPlayerControlMode::Character)
            {
                if (IMC_CharacterControls)
                {
                    InputSubsystem->AddMappingContext(IMC_CharacterControls, 0);
                    UE_LOG(LogSolaraqSystem, Log, TEXT("Applied CHARACTER Input Mapping Context: %s"), *IMC_CharacterControls->GetName());
                }
                else UE_LOG(LogSolaraqSystem, Error, TEXT("ASolaraqPlayerController: IMC_CharacterControls is not assigned! Input will not work."));
            }
        }
         else UE_LOG(LogSolaraqSystem, Error, TEXT("Failed to get EnhancedInputLocalPlayerSubsystem!"));
    }
     else UE_LOG(LogSolaraqSystem, Error, TEXT("Failed to get LocalPlayer!"));
}

void ASolaraqPlayerController::SwitchToMode(EPlayerControlMode NewMode)
{
    APawn* CurrentPawnReference = GetPawn(); // Get current pawn before any unpossess calls

    if (NewMode == CurrentControlMode && CurrentPawnReference)
    {
        UE_LOG(LogSolaraqSystem, Log, TEXT("Already in target mode: %s with pawn %s"), (NewMode == EPlayerControlMode::Ship ? TEXT("Ship") : TEXT("Character")), *CurrentPawnReference->GetName());
        ApplyInputContextForCurrentMode(); // Re-apply context just in case it got cleared
        return;
    }

    UE_LOG(LogSolaraqSystem, Log, TEXT("SwitchToMode requested: %s (Current: %s)"), 
        (NewMode == EPlayerControlMode::Ship ? TEXT("Ship") : TEXT("Character")),
        (CurrentControlMode == EPlayerControlMode::Ship ? TEXT("Ship") : TEXT("Character")));

    FTransform SpawnTransform;
    if (CurrentPawnReference)
    {
        SpawnTransform = CurrentPawnReference->GetActorTransform();
    }
    else if (PlayerCameraManager) // PlayerCameraManager is a member of APlayerController
    {
        SpawnTransform.SetLocation(PlayerCameraManager->GetCameraLocation() + PlayerCameraManager->GetActorForwardVector() * 100.f + FVector(0,0,100.f)); // Spawn in front of camera view
        SpawnTransform.SetRotation(PlayerCameraManager->GetCameraRotation().Quaternion());
    }
    else // Absolute fallback if no pawn and no camera manager (should be rare)
    {
        SpawnTransform = FTransform(FRotator::ZeroRotator, FVector::ZeroVector + FVector(0,0,100.f)); // Spawn at origin + offset
        UE_LOG(LogSolaraqSystem, Warning, TEXT("SwitchToMode: No CurrentPawnReference and no PlayerCameraManager. Spawning at world origin offset."));
    }

    if (NewMode == EPlayerControlMode::Character)
    {
        if (!CharacterPawnClass)
        {
            UE_LOG(LogSolaraqSystem, Error, TEXT("Cannot switch to Character mode: CharacterPawnClass not set in PlayerController!"));
            return;
        }

        // If we have a ship pawn and it's the one currently possessed, cache it.
        if (PossessedShipPawn && CurrentPawnReference == PossessedShipPawn)
        {
             // Spawn slightly behind/offset from the ship
            SpawnTransform.SetLocation(PossessedShipPawn->GetActorLocation() - PossessedShipPawn->GetActorForwardVector() * 200.f + FVector(0,0,50)); // Adjust offset as needed
            SpawnTransform.SetRotation(PossessedShipPawn->GetActorQuat()); // Match ship's orientation initially
        }
        
        UnPossess(); // Unpossess current pawn (ship or old character)

        // Destroy old character if it exists and wasn't the one we just unpossessed
        if (IsValid(PossessedCharacterPawn) && PossessedCharacterPawn != CurrentPawnReference)
        {
            PossessedCharacterPawn->Destroy();
        }
        PossessedCharacterPawn = nullptr; // Clear previous character pawn

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = nullptr;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        PossessedCharacterPawn = GetWorld()->SpawnActor<ASolaraqCharacterPawn>(CharacterPawnClass, SpawnTransform, SpawnParams);

        if (PossessedCharacterPawn)
        {
            Possess(PossessedCharacterPawn); // This will call OnPossess and set CurrentControlMode
        }
        else
        {
            UE_LOG(LogSolaraqSystem, Error, TEXT("Failed to spawn CharacterPawn!"));
            // Attempt to re-possess original ship if it exists
            if (IsValid(PossessedShipPawn))
            {
                Possess(PossessedShipPawn); // This will call OnPossess and set mode back to Ship
            }
            return; 
        }
    }
    else // NewMode == EPlayerControlMode::Ship
    {
        if (!IsValid(PossessedShipPawn))
        {
            UE_LOG(LogSolaraqSystem, Error, TEXT("Cannot switch to Ship mode: Original PossessedShipPawn is invalid or not set!"));
            // If we were controlling a character, try to destroy it.
            if (IsValid(PossessedCharacterPawn) && CurrentPawnReference == PossessedCharacterPawn)
            {
                UnPossess();
                PossessedCharacterPawn->Destroy();
            }
            PossessedCharacterPawn = nullptr;
            return;
        }
        
        UnPossess(); // Unpossess current character pawn

        // Destroy the character pawn when returning to ship
        if (IsValid(PossessedCharacterPawn))
        {
            PossessedCharacterPawn->Destroy();
        }
        PossessedCharacterPawn = nullptr;
        
        Possess(PossessedShipPawn); // This will call OnPossess and set CurrentControlMode
    }
    // ApplyInputContextForCurrentMode is called within OnPossess
}

// --- Input Handler Implementations ---

void ASolaraqPlayerController::HandleTogglePawnModeInput()
{
    UE_LOG(LogSolaraqSystem, Log, TEXT("HandleTogglePawnModeInput: Current mode is %s"), CurrentControlMode == EPlayerControlMode::Ship ? TEXT("Ship") : TEXT("Character"));
    if (CurrentControlMode == EPlayerControlMode::Ship)
    {
        SwitchToMode(EPlayerControlMode::Character);
    }
    else
    {
        SwitchToMode(EPlayerControlMode::Ship);
    }
}

void ASolaraqPlayerController::HandleCharacterMoveInput(const FInputActionValue& Value)
{
    if (CurrentControlMode == EPlayerControlMode::Character && PossessedCharacterPawn)
    {
        const FVector2D MovementVector = Value.Get<FVector2D>(); // Assuming IA_CharacterMove is Axis2D
        PossessedCharacterPawn->HandleMoveInput(MovementVector);
    }
}

// --- Helper to get controlled pawn (Modified/New) ---
ASolaraqShipBase* ASolaraqPlayerController::GetControlledShip() const
{
    // Log this specifically when called from a client trying to handle input
    if (GetNetMode() == NM_Client)
    {
        UE_LOG(LogSolaraqMovement, Warning, TEXT("CLIENT PC %s: GetControlledShip() called. CurrentControlMode: %s, PossessedShipPawn: %s"),
            *GetNameSafe(this),
            (CurrentControlMode == EPlayerControlMode::Ship ? TEXT("Ship") : (CurrentControlMode == EPlayerControlMode::Character ? TEXT("Character") : TEXT("UNKNOWN"))),
            *GetNameSafe(PossessedShipPawn));
    }
    
    // Directly return the cached possessed ship if in ship mode
    // This is updated in OnPossess and SwitchToMode
    if (CurrentControlMode == EPlayerControlMode::Ship)
    {
        return PossessedShipPawn;
    }
    return nullptr;
}

ASolaraqCharacterPawn* ASolaraqPlayerController::GetControlledCharacter() const
{
    if (CurrentControlMode == EPlayerControlMode::Character)
    {
        return PossessedCharacterPawn;
    }
    return nullptr;
}

void ASolaraqPlayerController::InitiateLevelTransitionToCharacter(FName TargetLevelName, FName DockingPadID)
{
    ASolaraqShipBase* CurrentShip = GetControlledShip();
    if (!CurrentShip)
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("PlayerController: Cannot transition, no controlled ship."));
        return;
    }

    USolaraqGameInstance* GI = GetGameInstance<USolaraqGameInstance>();
    if (!GI)
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("PlayerController: Cannot transition, GameInstance is invalid."));
        return;
    }

    UE_LOG(LogSolaraqSystem, Log, TEXT("PlayerController: Initiating level transition to Character Level: %s (from Pad: %s)"), *TargetLevelName.ToString(), *DockingPadID.ToString());

    // Store necessary info in GameInstance
    FString CurrentLevelName = GetWorld()->GetName();
    GI->PrepareForCharacterLevelLoad(TargetLevelName, CurrentShip->GetActorTransform(), FName(*CurrentLevelName), DockingPadID);

    // Unpossess the ship BEFORE loading the new level
    UnPossess(); 
    if (IsValid(PossessedShipPawn))
    {
        // Don't destroy the ship, it stays in the space level.
        // We just clear our reference to it as the "actively possessed ship pawn" for the controller.
        // The GameInstance holds its transform.
    }
    PossessedShipPawn = nullptr; // Clear our direct reference, GI has the info

    // Load the new level
    UGameplayStatics::OpenLevel(this, TargetLevelName);
}

void ASolaraqPlayerController::InitiateLevelTransitionToShip(FName TargetShipLevelName)
{
    USolaraqGameInstance* GI = GetGameInstance<USolaraqGameInstance>();
    if (!GI)
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("PlayerController: Cannot transition to ship, GameInstance is invalid."));
        return;
    }

    UE_LOG(LogSolaraqSystem, Log, TEXT("PlayerController: Initiating level transition to Ship Level: %s"), *TargetShipLevelName.ToString());

    FString CurrentLevelName = GetWorld()->GetName();
    GI->PrepareForShipLevelLoad(TargetShipLevelName, FName(*CurrentLevelName));

    UnPossess(); // Unpossess character
    if (IsValid(PossessedCharacterPawn))
    {
        PossessedCharacterPawn->Destroy(); // Destroy character when leaving character level
    }
    PossessedCharacterPawn = nullptr;

    UGameplayStatics::OpenLevel(this, TargetShipLevelName);
}

// --- EXISTING SHIP INPUT HANDLERS (Ensure they check CurrentControlMode or use GetControlledShip()) ---
void ASolaraqPlayerController::HandleMoveInput(const FInputActionValue& Value)
{
    
    ASolaraqShipBase* Ship = GetControlledShip(); // Use the getter
    if (Ship)
    {
        const float MoveValue = Value.Get<float>();
        
        if (GetNetMode() == NM_Client) { // Log only on client before sending RPC
            UE_LOG(LogSolaraqMovement, Warning, TEXT("CLIENT PC %s: Attempting to call Server_SendMoveForwardInput on Ship %s with Value: %.2f"),
                *GetNameSafe(this), *GetNameSafe(Ship), MoveValue);
        }
        
        
        Ship->Server_SendMoveForwardInput(MoveValue);
    }
    else if (GetNetMode() == NM_Client)
    {
        UE_LOG(LogSolaraqMovement, Error, TEXT("CLIENT PC %s: HandleMoveInput: GetControlledShip() is NULL! Cannot send RPC."), *GetNameSafe(this));
    }
}

void ASolaraqPlayerController::HandleTurnInput(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        const float TurnValue = Value.Get<float>();
        Ship->Server_SendTurnInput(TurnValue);
    }
}

void ASolaraqPlayerController::HandleTurnCompleted(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        Ship->Server_SendTurnInput(0.0f);
    }
}

void ASolaraqPlayerController::HandleFireRequest()
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        Ship->Server_RequestFire();
    }
}

void ASolaraqPlayerController::HandleBoostStarted(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        Ship->Server_SetAttemptingBoost(true);
    }
}

void ASolaraqPlayerController::HandleBoostCompleted(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        Ship->Server_SetAttemptingBoost(false);
    }
}

void ASolaraqPlayerController::HandleFireMissileRequest(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship && bIsHomingLockActive && LockedHomingTargetActor.IsValid())
    {
        AActor* Target = LockedHomingTargetActor.Get();
        if (Target)
        {
            Ship->Server_RequestFireHomingMissileAtTarget(Target);
        }
    }
    else if (!bIsHomingLockActive || !LockedHomingTargetActor.IsValid())
    {
        //UE_LOG(LogSolaraqProjectile, Warning, TEXT("Cannot fire homing missile: Lock not active or no valid target locked."));
    }
}

void ASolaraqPlayerController::HandleToggleLock()
{
    if (CurrentControlMode != EPlayerControlMode::Ship) // Action only valid in ship mode
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("ToggleLock ignored: Not in Ship control mode."));
        if (bIsHomingLockActive) // If lock was active from ship mode, turn it off
        {
             bIsHomingLockActive = false; // Force deactivate
             GetWorldTimerManager().ClearTimer(TimerHandle_ScanTargets);
             PotentialHomingTargets.Empty();
             LockedHomingTargetIndex = -1;
             LockedHomingTargetActor = nullptr;
             ClearTargetWidgets();
        }
        return;
    }

    bIsHomingLockActive = !bIsHomingLockActive;
    UE_LOG(LogSolaraqMarker, Warning, TEXT("PlayerController: Homing Lock Mode Toggled: %s"), bIsHomingLockActive ? TEXT("ACTIVE") : TEXT("INACTIVE"));

    if (bIsHomingLockActive)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("Homing Lock ACTIVATED. Starting target scan timer."));
        UpdatePotentialTargets();
        GetWorldTimerManager().SetTimer(TimerHandle_ScanTargets, this, &ASolaraqPlayerController::UpdatePotentialTargets, HomingTargetScanInterval, true);
    }
    else
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("Homing Lock DEACTIVATED. Clearing scan timer and target data."));
        GetWorldTimerManager().ClearTimer(TimerHandle_ScanTargets);
        PotentialHomingTargets.Empty();
        LockedHomingTargetIndex = -1;
        LockedHomingTargetActor = nullptr;
        ClearTargetWidgets();
    }
}

void ASolaraqPlayerController::HandleSwitchTarget(const FInputActionValue& Value)
{
    if (CurrentControlMode != EPlayerControlMode::Ship || !bIsHomingLockActive)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("HandleSwitchTarget(): SwitchTarget ignored: Not in Ship mode or Homing lock not active."));
        return;
    }
    if (PotentialHomingTargets.Num() <= 1)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("SwitchTarget ignored: Not enough potential targets (%d)."), PotentialHomingTargets.Num());
        return;
    }

    const float SwitchValue = Value.Get<float>();
    if (FMath::IsNearlyZero(SwitchValue))
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("SwitchTarget ignored: SwitchValue is nearly zero."));
        return;
    }

    int32 Direction = FMath::Sign(SwitchValue);
    int32 CurrentIdx = LockedHomingTargetIndex; // Use a local var for calculation clarity
    int32 NumTargets = PotentialHomingTargets.Num();
    int32 NextIndex = (CurrentIdx + Direction + NumTargets) % NumTargets;

    UE_LOG(LogSolaraqMarker, Warning, TEXT("Switching Target: CurrentIndex: %d, Direction: %d, NumTargets: %d, NextIndex: %d"),
        CurrentIdx, Direction, NumTargets, NextIndex);
    
    SelectTargetByIndex(NextIndex);

    if(LockedHomingTargetActor.IsValid())
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("Successfully Switched Target to: %s (Index: %d)"), *LockedHomingTargetActor->GetName(), LockedHomingTargetIndex);
    }
}

void ASolaraqPlayerController::HandleInteractInput()
{
    UE_LOG(LogSolaraqTransition, Warning, TEXT("PC %s: HandleInteractInput TRIGGERED! CurrentControlMode: %s"),
        *GetNameSafe(this), (CurrentControlMode == EPlayerControlMode::Ship ? TEXT("Ship") : TEXT("Character")));
    
    if (CurrentControlMode == EPlayerControlMode::Ship)
    {
        ASolaraqShipBase* Ship = GetControlledShip();
        if (Ship && Ship->IsShipDocked()) // Only allow interaction if fully docked
        {
            // The ship itself will handle finding the pad and what "Interact" means
            Ship->RequestInteraction(); 
            UE_LOG(LogSolaraqTransition, Warning, TEXT("PlayerController: Sent Interact request to docked ship %s."), *Ship->GetName());
        }
        else if (Ship)
        {
            UE_LOG(LogSolaraqTransition, Warning, TEXT("PlayerController: Interact pressed, but ship %s is not docked."), *Ship->GetName());
        }
        else
        {
            UE_LOG(LogSolaraqTransition, Warning, TEXT("PlayerController: Interact pressed in Ship mode, but GetControlledShip() is NULL."));
        }
    }
    else if (CurrentControlMode == EPlayerControlMode::Character)
    {
        ASolaraqCharacterPawn* CharPawn = GetControlledCharacter();
        if (CharPawn)
        {
            // TODO: Implement character interaction logic (e.g., find nearby interactable actor)
            UE_LOG(LogSolaraqTransition, Warning, TEXT("PlayerController: Character Interact pressed. (Not yet implemented fully)"));
            // For now, let's add a temporary way to get back to the ship level for testing
            // This would normally be triggered by interacting with the ship's "door" in the character level.
            USolaraqGameInstance* GI = GetGameInstance<USolaraqGameInstance>();
            if (GI && GI->OriginLevelName != NAME_None) // Check if we have an origin level to return to
            {
                InitiateLevelTransitionToShip(GI->OriginLevelName);
            }
        }
    }
    else
    {
        UE_LOG(LogSolaraqTransition, Warning, TEXT("PlayerController: Interact pressed, but CurrentControlMode is unknown/invalid."));
    }
}


// --- Homing Lock System Functions (Ensure they are present and check CurrentControlMode if necessary) ---
// IMPORTANT: You need to copy your full implementations for these from your original SolaraqPlayerController.cpp
// I'm providing stubs here that include the CurrentControlMode check.

void ASolaraqPlayerController::UpdatePotentialTargets()
{
    UE_LOG(LogSolaraqMarker, Warning, TEXT("--- Begin UpdatePotentialTargets ---"));
    
    if (!bIsHomingLockActive)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("UpdatePotentialTargets: Homing lock not active. Aborting scan."));
        return; // Should not happen if timer is managed correctly, but safety check
    }
    
    ASolaraqShipBase* MyShip = GetControlledShip();
    if (!MyShip)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("UpdatePotentialTargets: Controlled ship is NULL. Aborting scan."));
        return;
    }

    UE_LOG(LogSolaraqMarker, Warning, TEXT("UpdatePotentialTargets: Scanning for player %s (Ship: %s)"), *GetName(), *MyShip->GetName());
    
    // Ensure MyShip implements IGenericTeamAgentInterface
    IGenericTeamAgentInterface* MyTeamAgent = Cast<IGenericTeamAgentInterface>(MyShip->GetController()); // Or MyShip itself if it directly holds the team ID
    if (!MyTeamAgent) MyTeamAgent = Cast<IGenericTeamAgentInterface>(MyShip); // Fallback to pawn

    if (!MyTeamAgent)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("Player's ship or controller does not implement IGenericTeamAgentInterface. Cannot determine attitude."));
        return; // Or handle as neutral/error
    }

    
    TArray<TWeakObjectPtr<AActor>> NewPotentialTargets;
    const FVector SelfLocation = MyShip->GetActorLocation();
    const FVector SelfForward = MyShip->GetActorForwardVector();
    const float MaxRangeSq = FMath::Square(HomingTargetScanRange);
    const float MinDotProduct = FMath::Cos(FMath::DegreesToRadians(HomingTargetScanConeAngleDegrees / 2.0f));

    UE_LOG(LogSolaraqMarker, Warning, TEXT("Scan Params: SelfLoc: %s, SelfFwd: %s, MaxRangeSq: %.1f, (Angle: %.1f deg)"),
        *SelfLocation.ToString(),
        *SelfForward.ToString(),
        MaxRangeSq,
        HomingTargetScanConeAngleDegrees);
    
    int32 IteratedActorsCount = 0;
    for (TActorIterator<ASolaraqShipBase> It(GetWorld()); It; ++It)
    {
        IteratedActorsCount++;
        ASolaraqShipBase* PotentialTargetShip = *It;
        if (!PotentialTargetShip)
        {
            UE_LOG(LogSolaraqMarker, Warning, TEXT("  Iter %d: PotentialTargetShip is NULL."), IteratedActorsCount);
            continue;
        }
        
        UE_LOG(LogSolaraqMarker, Warning, TEXT("  Iter %d: Checking %s."), IteratedActorsCount, *PotentialTargetShip->GetName());

        if (PotentialTargetShip == MyShip)
        {
            UE_LOG(LogSolaraqMarker, Warning, TEXT("    - Is self. Skipping."));
            continue;
        }

        if (PotentialTargetShip->IsDead())
        {
            UE_LOG(LogSolaraqMarker, Warning, TEXT("    - Is dead. Skipping."));
            continue;
        }

        // Ensure PotentialTargetShip or its controller implements IGenericTeamAgentInterface
        IGenericTeamAgentInterface* OtherTeamAgent = Cast<IGenericTeamAgentInterface>(PotentialTargetShip->GetController());
        if (!OtherTeamAgent) OtherTeamAgent = Cast<IGenericTeamAgentInterface>(PotentialTargetShip);

        if (!OtherTeamAgent)
        {
            // NET_LOG(LogSolaraqSystem, VeryVerbose, TEXT("Potential target %s or its controller does not implement IGenericTeamAgentInterface. Skipping."), *PotentialTargetShip->GetName());
            continue;
        }
        
        ETeamAttitude::Type Attitude = FGenericTeamId::GetAttitude(MyTeamAgent->GetGenericTeamId(), OtherTeamAgent->GetGenericTeamId());
        UE_LOG(LogSolaraqMarker, Warning, TEXT("    - Attitude towards %s: %s"), *PotentialTargetShip->GetName(), *UEnum::GetValueAsString(Attitude));
        if (Attitude != ETeamAttitude::Hostile)
        {
            UE_LOG(LogSolaraqMarker, Warning, TEXT("      - Not Hostile. Skipping."));
            continue;
        }

        UE_LOG(LogSolaraqMarker, Warning, TEXT("      - Is Hostile. Proceeding with checks."));
        
        const FVector TargetLocation = PotentialTargetShip->GetActorLocation();
        const float DistSq = FVector::DistSquared(SelfLocation, TargetLocation);

        UE_LOG(LogSolaraqMarker, Warning, TEXT("      - TargetLoc: %s, DistSq: %.1f (MaxRangeSq: %.1f)"), *TargetLocation.ToString(), DistSq, MaxRangeSq);

        if (DistSq < MaxRangeSq)
        {
            UE_LOG(LogSolaraqMarker, Warning, TEXT("        - Within MaxRange."));
            const FVector DirectionToTarget = (TargetLocation - SelfLocation).GetSafeNormal();
            const float DotToTarget = FVector::DotProduct(SelfForward, DirectionToTarget);
            UE_LOG(LogSolaraqMarker, Warning, TEXT("        - DirToTarget: %s, DotToTarget: %.3f (CosHalfAngle threshold: %.3f)"),
                            *DirectionToTarget.ToString(), DotToTarget, MinDotProduct);
            
            if (DotToTarget >= MinDotProduct) // Check if within cone
            {
                UE_LOG(LogSolaraqMarker, Warning, TEXT("          - QUALIFIED TARGET: %s. Adding to potential list."), *PotentialTargetShip->GetName());
                NewPotentialTargets.Add(PotentialTargetShip);
                
            }
            else
            {
                UE_LOG(LogSolaraqMarker, Warning, TEXT("          - Outside cone. Skipping."));
            }
        }
        else
        {
            UE_LOG(LogSolaraqMarker, Warning, TEXT("        - Outside MaxRange. Skipping."));
        }
    }
    
    UE_LOG(LogSolaraqMarker, Warning, TEXT("Scan Iteration Complete. Iterated over %d actors. Found %d NewPotentialTargets."), IteratedActorsCount, NewPotentialTargets.Num());

    // --- Update the list and handle lock persistence ---
    AActor* PreviouslyLockedActor = LockedHomingTargetActor.Get();
    PotentialHomingTargets = NewPotentialTargets; // Replace old list

    int32 FoundPreviousLockIndex = -1;
    if (PreviouslyLockedActor)
    {
        // Try to find the previously locked actor in the new list
        for (int32 i = 0; i < PotentialHomingTargets.Num(); ++i)
        {
            if (PotentialHomingTargets[i].Get() == PreviouslyLockedActor)
            {
                FoundPreviousLockIndex = i;
                break;
            }
        }
    }

    if (FoundPreviousLockIndex != -1)
    {
        // Previous target still valid, keep lock
        SelectTargetByIndex(FoundPreviousLockIndex);
        //UE_LOG(LogSolaraqSystem, Verbose, TEXT("Target Scan: Previous lock %s maintained at index %d."), *PreviouslyLockedActor->GetName(), FoundPreviousLockIndex);
    }
    else if (PotentialHomingTargets.Num() > 0)
    {
        // Previous target lost or none was locked, lock the first available one
        SelectTargetByIndex(0);
        //UE_LOG(LogSolaraqSystem, Verbose, TEXT("Target Scan: Previous lock lost or none existed. Locking first target at index 0: %s"), *GetNameSafe(LockedHomingTargetActor.Get()));
    }
    else
    {
        // No targets found
        SelectTargetByIndex(-1); // No lock
        //UE_LOG(LogSolaraqSystem, Verbose, TEXT("Target Scan: No potential targets found."));
    }

    // No need to call UpdateTargetWidgets here, Tick will handle it
}

void ASolaraqPlayerController::UpdateTargetWidgets()
{
    if (!IsLocalController())
    {
        return;
    }
    if (!TargetMarkerWidgetClass)
    {
        UE_LOG(LogSolaraqMarker, Error, TEXT("UpdateTargetWidgets: TargetMarkerWidgetClass is NULL! Cannot create markers."));
        return;
    }

    // UE_LOG(LogSolaraqMarker, Log, TEXT("--- UpdateTargetWidgets START --- PotentialTargets: %d, Current Widgets: %d, LockedTarget: %s"), PotentialHomingTargets.Num(), TargetMarkerWidgets.Num(), *GetNameSafe(LockedHomingTargetActor.Get()));

    TSet<TWeakObjectPtr<AActor>> CurrentTargetsOnScreen; // Keep track of targets that *could* have a widget
    FVector2D ScreenSize;
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(ScreenSize);
    }
    else
    {
        UE_LOG(LogSolaraqMarker, Error, TEXT("UpdateTargetWidgets: GEngine or GameViewport is NULL! Cannot get screen size."));
        return;
    }

    // Step 1: Create/Update/Position widgets for all potential targets that are on screen.
    //         Initially, we can set them all to hidden or a default "potential" state.
    for (int32 i = 0; i < PotentialHomingTargets.Num(); ++i)
    {
        TWeakObjectPtr<AActor> TargetPtr = PotentialHomingTargets[i];
        AActor* TargetActor = TargetPtr.Get();

        if (!TargetActor)
        {
            // UE_LOG(LogSolaraqMarker, Verbose, TEXT("  TargetActor at index %d is NULL or Stale. Skipping."), i);
            continue;
        }

        FVector TargetLocation = TargetActor->GetActorLocation();
        FVector2D ScreenPosition;

        if (UGameplayStatics::ProjectWorldToScreen(this, TargetLocation, ScreenPosition, false))
        {
            // Check if roughly on screen
            if (ScreenPosition.X >= 0 && ScreenPosition.X <= ScreenSize.X && ScreenPosition.Y >= 0 && ScreenPosition.Y <= ScreenSize.Y)
            {
                CurrentTargetsOnScreen.Add(TargetPtr); // Mark this target as being on screen

                TObjectPtr<UUserWidget>* FoundWidgetPtr = TargetMarkerWidgets.Find(TargetPtr);
                UUserWidget* WidgetInstance = (FoundWidgetPtr && *FoundWidgetPtr) ? (*FoundWidgetPtr).Get() : nullptr;

                if (!WidgetInstance)
                {
                    // UE_LOG(LogSolaraqMarker, Log, TEXT("  Creating widget for %s"), *TargetActor->GetName());
                    UUserWidget* NewWidgetInstance = CreateWidget<UUserWidget>(this, TargetMarkerWidgetClass);
                    if (NewWidgetInstance)
                    {
                        NewWidgetInstance->AddToViewport();
                        TargetMarkerWidgets.Add(TargetPtr, NewWidgetInstance);
                        WidgetInstance = NewWidgetInstance;
                    }
                    else
                    {
                        UE_LOG(LogSolaraqMarker, Error, TEXT("  FAILED to create TargetMarkerWidgetClass instance for %s!"), *TargetActor->GetName());
                        continue;
                    }
                }

                // Update position for all on-screen potential target widgets
                WidgetInstance->SetPositionInViewport(ScreenPosition, true);

                // --- Visibility Logic ---
                // Check if this is the currently *locked* target
                if (LockedHomingTargetActor.Get() == TargetActor)
                {
                    WidgetInstance->SetVisibility(ESlateVisibility::HitTestInvisible); // Make it visible and non-interactive
                    // Optionally, also change its color to make it stand out more
                    // WidgetInstance->SetColorAndOpacity(FLinearColor::Red);
                    // UE_LOG(LogSolaraqMarker, Verbose, TEXT("  Widget for %s is LOCKED. Set Visible."), *TargetActor->GetName());
                }
                else
                {
                    WidgetInstance->SetVisibility(ESlateVisibility::Hidden); // Hide widgets for non-locked targets
                    // Or if you want them visible but different:
                    // WidgetInstance->SetVisibility(ESlateVisibility::HitTestInvisible);
                    // WidgetInstance->SetColorAndOpacity(FLinearColor::MakeRandomColor()); // Or some other "potential" color
                    // UE_LOG(LogSolaraqMarker, Verbose, TEXT("  Widget for %s is NOT locked. Set Hidden."), *TargetActor->GetName());
                }
            }
            else // Target projected but is off-screen
            {
                 // If it was on screen before (i.e., we have a widget for it), hide it or remove it
                if (TObjectPtr<UUserWidget>* FoundWidgetPtr = TargetMarkerWidgets.Find(TargetPtr))
                {
                    if (UUserWidget* WidgetToRemove = FoundWidgetPtr->Get())
                    {
                        WidgetToRemove->SetVisibility(ESlateVisibility::Hidden); // Hide it
                        // Or remove it entirely if you prefer:
                        // WidgetToRemove->RemoveFromParent();
                        // TargetMarkerWidgets.Remove(TargetPtr); // And remove from map
                        // UE_LOG(LogSolaraqMarker, Verbose, TEXT("  Widget for %s is OFF-SCREEN. Set Hidden or Removed."), *GetNameSafe(TargetActor));
                    }
                }
            }
        }
        else // Target projection failed (e.g., behind player)
        {
             // If it was on screen before, hide it or remove it
            if (TObjectPtr<UUserWidget>* FoundWidgetPtr = TargetMarkerWidgets.Find(TargetPtr))
            {
                if (UUserWidget* WidgetToRemove = FoundWidgetPtr->Get())
                {
                    WidgetToRemove->SetVisibility(ESlateVisibility::Hidden); // Hide it
                    // Or remove it entirely:
                    // WidgetToRemove->RemoveFromParent();
                    // TargetMarkerWidgets.Remove(TargetPtr);
                    // UE_LOG(LogSolaraqMarker, Verbose, TEXT("  Widget for %s PROJECTION FAILED. Set Hidden or Removed."), *GetNameSafe(TargetActor));
                }
            }
        }
    }

    // Step 2: Clean up widgets for targets that are no longer in PotentialHomingTargets list
    // or were on screen but are no longer valid.
    TArray<TWeakObjectPtr<AActor>> WidgetsToRemoveKeys;
    for (auto It = TargetMarkerWidgets.CreateIterator(); It; ++It)
    {
        TWeakObjectPtr<AActor> TargetKey = It.Key();
        UUserWidget* WidgetInstance = It.Value();

        if (!WidgetInstance) // Should not happen if map is managed well
        {
            WidgetsToRemoveKeys.Add(TargetKey);
            continue;
        }

        bool bStillPotentialAndValid = false;
        if (TargetKey.IsValid()) // Check if the key itself is still a valid actor pointer
        {
            for (const auto& PotentialTarget : PotentialHomingTargets)
            {
                if (PotentialTarget == TargetKey)
                {
                    bStillPotentialAndValid = true;
                    break;
                }
            }
        }

        // If the target is no longer potential OR no longer on screen (CurrentTargetsOnScreen only contains on-screen ones from this frame)
        if (!bStillPotentialAndValid || !CurrentTargetsOnScreen.Contains(TargetKey))
        {
            // UE_LOG(LogSolaraqMarker, Log, TEXT("  Removing widget for STALE/OFF-SCREEN target %s"), *GetNameSafe(TargetKey.Get()));
            WidgetInstance->RemoveFromParent();
            WidgetsToRemoveKeys.Add(TargetKey); // Mark for removal from map
        }
    }

    for (const auto& KeyToRemove : WidgetsToRemoveKeys)
    {
        TargetMarkerWidgets.Remove(KeyToRemove);
    }
    // UE_LOG(LogSolaraqMarker, Log, TEXT("--- UpdateTargetWidgets END --- Widgets in map: %d"), TargetMarkerWidgets.Num());
}

void ASolaraqPlayerController::ClearTargetWidgets()
{
    for (auto const& [TargetPtr, WidgetInstance] : TargetMarkerWidgets)
    {
        if (WidgetInstance)
        {
            WidgetInstance->RemoveFromParent();
        }
    }
    TargetMarkerWidgets.Empty();
}

void ASolaraqPlayerController::SelectTargetByIndex(int32 Index)
{
    if (CurrentControlMode != EPlayerControlMode::Ship)
    {
        AActor* PreviouslyLocked = LockedHomingTargetActor.Get();
        LockedHomingTargetActor = nullptr;
        LockedHomingTargetIndex = -1;
        
        // If there was a previously locked target, update its widget state
        if (PreviouslyLocked)
        {
            TObjectPtr<UUserWidget>* FoundWidget = TargetMarkerWidgets.Find(PreviouslyLocked);
            if (FoundWidget && FoundWidget->Get())
            {
                if (ITargetWidgetInterface* TargetWidget = Cast<ITargetWidgetInterface>(FoundWidget->Get()))
                {
                    TargetWidget->Execute_SetLockedState(FoundWidget->Get(), false); // << CORRECTED CALL
                }
                // Optionally hide it too, UpdateTargetWidgets will handle this in the next Tick if logic is correct
                // FoundWidget->Get()->SetVisibility(ESlateVisibility::Hidden); 
            }
        }
        return;
    }
    
    UE_LOG(LogSolaraqMarker, Log, TEXT("SelectTargetByIndex called with Index: %d. PotentialHomingTargets count: %d"), Index, PotentialHomingTargets.Num());

    AActor* PreviousLock = LockedHomingTargetActor.Get();

    if (Index >= 0 && Index < PotentialHomingTargets.Num())
    {
        // Valid index
        TWeakObjectPtr<AActor> NewTargetPtr = PotentialHomingTargets[Index];
        if (NewTargetPtr.IsValid())
        {
            LockedHomingTargetActor = NewTargetPtr;
            LockedHomingTargetIndex = Index;
            if (LockedHomingTargetActor.Get() != PreviousLock) // Log only if target actually changed
            {
                UE_LOG(LogSolaraqMarker, Log, TEXT("Selected new target: %s (Index: %d)"), *LockedHomingTargetActor->GetName(), LockedHomingTargetIndex);
            }
        }
        else
        {
            // Target at this index is stale/invalid
            UE_LOG(LogSolaraqMarker, Warning, TEXT("Target at index %d is stale or invalid. Clearing lock."), Index);
            LockedHomingTargetActor = nullptr;
            LockedHomingTargetIndex = -1;
        }
    }
    else
    {
        // Index is out of bounds (e.g., -1 for no target, or list is empty)
        if (LockedHomingTargetActor.IsValid() || LockedHomingTargetIndex != -1) // Log only if lock state changes
        {
            UE_LOG(LogSolaraqMarker, Log, TEXT("Index %d is invalid or no targets. Clearing lock."), Index);
        }
        LockedHomingTargetActor = nullptr;
        LockedHomingTargetIndex = -1;
    }

    // UpdateTargetWidgets is called in Tick if bIsHomingLockActive is true,
    // which will pick up the change to LockedHomingTargetActor and re-style widgets.
    // No need to call it directly here unless specific immediate feedback is required before the next Tick.
}