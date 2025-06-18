// SolaraqCharacterPlayerController.cpp

#include "Controllers/SolaraqCharacterPlayerController.h" // Adjust to your path
#include "Pawns/SolaraqCharacterPawn.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "NavigationSystem.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Core/SolaraqGameInstance.h" // For level transition
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h" // For OpenLevel
#include "Logging/SolaraqLogChannels.h"
#include "Systems/FishingSubsystem.h"

ASolaraqCharacterPlayerController::ASolaraqCharacterPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    DefaultMouseCursor = EMouseCursor::Default;
    CachedDestination = FVector::ZeroVector;
}

ASolaraqCharacterPawn* ASolaraqCharacterPlayerController::GetControlledCharacter() const
{
    return Cast<ASolaraqCharacterPawn>(GetPawn());
}

void ASolaraqCharacterPlayerController::ShowFishingHUD()
{
    if (!FishingHUDWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("FishingHUDWidgetClass is not set in the PlayerController Blueprint!"));
        return;
    }

    if (!FishingHUDWidgetInstance)
    {
        FishingHUDWidgetInstance = CreateWidget<UUserWidget>(this, FishingHUDWidgetClass);
    }
    
    if (FishingHUDWidgetInstance && !FishingHUDWidgetInstance->IsInViewport())
    {
        FishingHUDWidgetInstance->AddToViewport();
    }
}

void ASolaraqCharacterPlayerController::HideFishingHUD()
{
    if (FishingHUDWidgetInstance && FishingHUDWidgetInstance->IsInViewport())
    {
        FishingHUDWidgetInstance->RemoveFromParent();
        // We can let it be garbage collected or null it out if we want to be explicit
        // For this simple case, just removing it is fine. It will be re-used next time.
    }
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

    FInputModeGameAndUI InputModeData;
    InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputModeData.SetHideCursorDuringCapture(false);
    SetInputMode(InputModeData);
    
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
        if (USpringArmComponent* SpringArm = PossessedChar->GetSpringArmComponent())
        {
            // Sync our target length with the pawn's default to prevent a "snap" on possess.
            TargetZoomLength = SpringArm->TargetArmLength;
        }
        
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
    if (CameraZoomAction)
    {
        EnhancedInputComponentRef->BindAction(CameraZoomAction, ETriggerEvent::Triggered, this, &ASolaraqCharacterPlayerController::HandleCameraZoom);
    }
    if (PointerMoveAction)
    {
        // We bind ONE function. The triggers in the IMC will determine WHEN it gets called.
        // ETriggerEvent::Triggered works for both Tap (on release) and Hold (continuously).
        EnhancedInputComponentRef->BindAction(PointerMoveAction, ETriggerEvent::Triggered, this, &ASolaraqCharacterPlayerController::HandlePointerMove);
    }
    if (ToggleFishingModeAction)
    {
        EnhancedInputComponentRef->BindAction(ToggleFishingModeAction, ETriggerEvent::Started, this, &ASolaraqCharacterPlayerController::HandleToggleFishingMode);
    }
}

void ASolaraqCharacterPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (ASolaraqCharacterPawn* CharPawn = GetControlledCharacter())
    {
        if (USpringArmComponent* SpringArm = CharPawn->GetSpringArmComponent())
        {
            bool bIsInFishingMode_ThisFrame = false;
            if (UFishingSubsystem* FishingSS = GetWorld()->GetSubsystem<UFishingSubsystem>())
            {
                bIsInFishingMode_ThisFrame = (FishingSS->GetCurrentState() != EFishingState::Idle);
            }

            // --- State Transition Logic ---
            if (bIsInFishingMode_ThisFrame && !bWasInFishingMode_LastFrame)
            {
                PreFishingZoomLength = TargetZoomLength;
            }
            else if (!bIsInFishingMode_ThisFrame && bWasInFishingMode_LastFrame)
            {
                TargetZoomLength = PreFishingZoomLength;
            }

            // --- Continuous State Logic ---
            if (bIsInFishingMode_ThisFrame)
            {
                // While in fishing mode:
                TargetZoomLength = FishingModeZoomLength;
                TargetCameraOffset = CharPawn->GetTargetAimingRotation().Vector() * CharPawn->FishingCameraRadius;
                
                // Reset custom lag state when entering fishing mode
                CurrentCameraTargetOffset = FVector::ZeroVector;
                bIsInForcedRejoinState = false;
                TimeAtMaxOffset = 0.f;
            }
            else // --- NOT FISHING: USE CUSTOM LOOK-AHEAD LOGIC ---
            {
                if (bUseCustomCameraLag)
                {
                    const FVector CharacterVelocity = CharPawn->GetVelocity();
                    const FVector VelocityDirection = CharacterVelocity.GetSafeNormal();

                    if (CharacterVelocity.SizeSquared() > 1.f) // If character is moving
                    {
                        // Check for significant direction change to reset rejoin logic
                        if (FVector::DotProduct(VelocityDirection, LastMovementDirection) < RejoinDirectionChangeThreshold)
                        {
                            bIsInForcedRejoinState = false;
                            TimeAtMaxOffset = 0.0f;
                        }

                        if (bIsInForcedRejoinState)
                        {
                            // We are forcing the camera to shrink back towards the character.
                            // It will stay in this state until the player stops or changes direction.
                            if (RejoinInterpolationMethod == ERejoinInterpolationType::Linear)
                            {
                                CurrentCameraTargetOffset = FMath::VInterpConstantTo(CurrentCameraTargetOffset, FVector::ZeroVector, DeltaTime, CameraForcedRejoinSpeed_Linear);
                            }
                            else // InterpTo
                            {
                                CurrentCameraTargetOffset = FMath::VInterpTo(CurrentCameraTargetOffset, FVector::ZeroVector, DeltaTime, CameraForcedRejoinSpeed_Interp);
                            }

                            // --- FIX: REMOVED THE PREMATURE EXIT CONDITION ---
                            // The logic to exit the rejoin state is now solely handled by the player
                            // stopping or changing direction.
                        }
                        else // Normal look-ahead behavior
                        {
                            const FVector DesiredOffset = VelocityDirection * CameraLookAheadFactor;
                            CurrentCameraTargetOffset = FMath::VInterpTo(CurrentCameraTargetOffset, DesiredOffset, DeltaTime, CustomCameraLagSpeed);

                            if (FMath::IsNearlyEqual(CurrentCameraTargetOffset.Size(), MaxCameraTargetOffset, 1.0f))
                            {
                                CurrentCameraTargetOffset = CurrentCameraTargetOffset.GetSafeNormal() * MaxCameraTargetOffset;
                                
                                TimeAtMaxOffset += DeltaTime;
                                if (TimeAtMaxOffset >= DelayBeforeForcedRejoin)
                                {
                                    bIsInForcedRejoinState = true;
                                    DirectionWhenForcedRejoinStarted = VelocityDirection;
                                }
                            }
                            else
                            {
                                TimeAtMaxOffset = 0.0f;
                            }
                        }
                        LastMovementDirection = VelocityDirection;
                    }
                    else // If character is standing still
                    {
                        // Reset all states and recenter the camera
                        bIsInForcedRejoinState = false;
                        TimeAtMaxOffset = 0.0f;
                        LastMovementDirection = FVector::ZeroVector;
                        CurrentCameraTargetOffset = FMath::VInterpTo(CurrentCameraTargetOffset, FVector::ZeroVector, DeltaTime, CameraRecenteringSpeed);
                    }
                    
                    TargetCameraOffset = CurrentCameraTargetOffset;
                }
                else // bUseCustomCameraLag is false
                {
                    TargetCameraOffset = FVector::ZeroVector;
                }
            }

            // --- UNIVERSAL INTERPOLATION (applies to all states) ---
            SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, TargetZoomLength, DeltaTime, ZoomInterpSpeed);
            SpringArm->TargetOffset = FMath::VInterpTo(SpringArm->TargetOffset, TargetCameraOffset, DeltaTime, CameraOffsetInterpSpeed);

            if (CameraZoomCurve)
            {
                const float TargetPitch = CameraZoomCurve->GetFloatValue(SpringArm->TargetArmLength);
                const FRotator CurrentRotation = SpringArm->GetRelativeRotation();
                const FRotator TargetRotation = FRotator(TargetPitch * -1.f, CurrentRotation.Yaw, CurrentRotation.Roll);
                SpringArm->SetRelativeRotation(FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationInterpSpeed));
            }

            bWasInFishingMode_LastFrame = bIsInFishingMode_ThisFrame;
        }
    }
}

void ASolaraqCharacterPlayerController::HandleCharacterMoveInput(const FInputActionValue& Value)
{
    if (UFishingSubsystem* FishingSubsystem = GetWorld()->GetSubsystem<UFishingSubsystem>())
    {
        if (FishingSubsystem->GetCurrentState() != EFishingState::Idle)
        {
            UE_LOG(LogSolaraqFishing, Log, TEXT("PC: Movement input detected, cancelling fishing."));
            FishingSubsystem->ResetState();
        }
    }
    
    StopMovement();
    
    ASolaraqCharacterPawn* CharPawn = GetControlledCharacter();
    if (CharPawn)
    {
        const FVector2D MovementVector = Value.Get<FVector2D>();
        CharPawn->HandleMoveInput(MovementVector);
    }
}

void ASolaraqCharacterPlayerController::HandlePointerMove(const FInputActionValue& Value)
{
    FHitResult Hit;
    if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
    {
        if (Hit.bBlockingHit)
        {
            // This function now handles both taps and holds seamlessly.
            // It simply moves to wherever the cursor is when a valid trigger fires.
            MoveToDestination(Hit.Location);
        }
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
    ASolaraqCharacterPawn* CharPawn = GetControlledCharacter();

    if (CharPawn)
    {
        if (CharPawn->GetVelocity().SizeSquared() > 1.0f)
        {
            if (UFishingSubsystem* FishingSS = GetWorld()->GetSubsystem<UFishingSubsystem>())
            {
                const EFishingState CurrentFishingState = FishingSS->GetCurrentState();
                // Allow reeling while moving slightly, but not starting a new cast.
                if (CurrentFishingState == EFishingState::Idle || CurrentFishingState == EFishingState::ReadyToCast)
                {
                    UE_LOG(LogSolaraqFishing, Log, TEXT("PC: PrimaryUse blocked because pawn is moving."));
                    return; // Abort!
                }
            }
        }
    
        UE_LOG(LogSolaraqFishing, Warning, TEXT("PC: HandlePrimaryUseStarted() - Input received."));
    
        if (UEquipmentComponent* EquipComp = CharPawn->GetEquipmentComponent())
        {
            EquipComp->HandlePrimaryUse(); // Pass the command to the pawn's component
        }
    }
    
}

void ASolaraqCharacterPlayerController::HandlePrimaryUseCompleted()
{
    UE_LOG(LogSolaraqFishing, Warning, TEXT("PC: HandlePrimaryUseCompleted() - Input received."));
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

void ASolaraqCharacterPlayerController::HandleToggleFishingMode()
{
    ASolaraqCharacterPawn* CharPawn = GetControlledCharacter();
    if (!CharPawn) return;

    // --- NEW PRE-EMPTIVE CHECK ---
    // You cannot enter fishing mode if you are moving.
    if (CharPawn->GetVelocity().SizeSquared() > 1.0f)
    {
        UE_LOG(LogSolaraqFishing, Log, TEXT("PC: ToggleFishingMode blocked because pawn is moving."));
        return; // Abort!
    }
    // --- END OF CHECK ---

    if (UFishingSubsystem* FishingSubsystem = GetWorld()->GetSubsystem<UFishingSubsystem>())
    {
        FishingSubsystem->RequestToggleFishingMode(GetControlledCharacter());
    }
}

void ASolaraqCharacterPlayerController::HandleCameraZoom(const FInputActionValue& Value)
{
    const float ZoomAxisValue = Value.Get<float>();
    if (FMath::IsNearlyZero(ZoomAxisValue))
    {
        return; // No input
    }

    // We no longer directly set the spring arm length.
    // We just update our target, and Tick() will handle the interpolation.
    TargetZoomLength -= ZoomAxisValue * ZoomStepAmount;
    TargetZoomLength = FMath::Clamp(TargetZoomLength, MinZoomLength, MaxZoomLength);
}

void ASolaraqCharacterPlayerController::MoveToDestination(const FVector& Destination)
{
    if (APawn* ControlledPawn = GetPawn())
    {
        // Stop any active fishing actions if we issue a move command
        if(UFishingSubsystem* FishingSubsystem = GetWorld()->GetSubsystem<UFishingSubsystem>())
        {
            if(FishingSubsystem->GetCurrentState() != EFishingState::Idle)
            {
                FishingSubsystem->ResetState();
            }
        }

        if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
        {
            UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Destination);
        }
    }
}
