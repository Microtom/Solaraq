// SolaraqCharacterPlayerController.cpp

#include "Controllers/SolaraqCharacterPlayerController.h" 
#include "Pawns/SolaraqCharacterPawn.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "NavigationSystem.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Core/SolaraqGameInstance.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/SolaraqLogChannels.h"
#include "Systems/FishingSubsystem.h"
#include "UI/SolaraqHUDWidget.h"
#include "Items/SolaraqEquipmentComponent.h"
#include "UI/Inventory/SolaraqEquipmentWindowWidget.h"
#include "UI/Inventory/SolaraqInventoryWindowWidget.h"
#include "UI/Inventory/SolaraqInventoryGridWidget.h"
#include "UI/Inventory/SolaraqContainerWindowWidget.h"
#include "Actors/Interactables/SolaraqContainerBase.h"

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
    }
}

void ASolaraqCharacterPlayerController::RequestMoveToInteract(AActor* TargetActor, FVector TargetLocation,
    float AcceptanceRadius)
{
    if (!TargetActor) return;

    PendingInteractableActor = TargetActor;
    PendingInteractionLocation = TargetLocation;
    PendingInteractionRadius = AcceptanceRadius;
    bIsAutoNavigatingToInteract = true;

    UE_LOG(LogSolaraqSystem, Log, TEXT("RequestMoveToInteract: Moving to %s with Radius %.2f"), *TargetLocation.ToString(), AcceptanceRadius);

    // Call MoveTo ONLY ONCE here. Do not spam it in Tick.
    UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, PendingInteractionLocation);
}

void ASolaraqCharacterPlayerController::ApplyCharacterInputMappingContext()
{
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            ClearAllInputContexts(InputSubsystem); 

            if (IMC_CharacterControls)
            {
                AddInputContext(InputSubsystem, IMC_CharacterControls, 0); 
                UE_LOG(LogSolaraqSystem, Log, TEXT("ASolaraqCharacterPlayerController: Applied CHARACTER Input Mapping Context: %s"), *IMC_CharacterControls->GetName());
            }
            else
            {
                UE_LOG(LogSolaraqSystem, Error, TEXT("ASolaraqCharacterPlayerController: IMC_CharacterControls is not assigned! Character input will not work."));
            }
        }
    }
}

void ASolaraqCharacterPlayerController::CreateHUD()
{
    if (MainHUDWidgetInstance) return;

    if (!MainHUDWidgetClass)
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("CreateHUD FAILED: MainHUDWidgetClass is not set in the PlayerController Blueprint!"));
        return;
    }

    UE_LOG(LogSolaraqSystem, Log, TEXT("CreateHUD: Attempting to create widget of class %s."), *MainHUDWidgetClass->GetName());
	
    MainHUDWidgetInstance = CreateWidget<USolaraqHUDWidget>(this, MainHUDWidgetClass);

    if (MainHUDWidgetInstance)
    {
        UE_LOG(LogSolaraqSystem, Log, TEXT("CreateHUD: Widget created successfully. Adding to viewport."));
        MainHUDWidgetInstance->AddToViewport();
    }
    else
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("CreateHUD FAILED: CreateWidget returned NULL."));
    }
}

void ASolaraqCharacterPlayerController::BeginPlay()
{
    Super::BeginPlay();
    
    FInputModeGameAndUI InputModeData;
    InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputModeData.SetHideCursorDuringCapture(false);
    SetInputMode(InputModeData);
    
    if (GetPawn()) 
    {
        ApplyCharacterInputMappingContext();
    }
}

void ASolaraqCharacterPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    ASolaraqCharacterPawn* PossessedChar = Cast<ASolaraqCharacterPawn>(InPawn);
    FString AuthorityPrefix = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");

    if (IsLocalPlayerController())
    {
        CreateHUD();
    }
    
    if (PossessedChar)
    {
        if (USpringArmComponent* SpringArm = PossessedChar->GetSpringArmComponent())
        {
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
    if (MainHUDWidgetInstance)
    {
        MainHUDWidgetInstance->RemoveFromParent();
        MainHUDWidgetInstance = nullptr;
    }
    
    FString AuthorityPrefix = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
    APawn* UnpossessedPawn = GetPawn(); 
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
    ApplyCharacterInputMappingContext(); 
}

void ASolaraqCharacterPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent(); 

    if (!EnhancedInputComponentRef)
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("ASolaraqCharacterPlayerController: EnhancedInputComponentRef is NULL after Super::SetupInputComponent! Cannot bind character actions."));
        return;
    }
    
    UE_LOG(LogSolaraqSystem, Log, TEXT("ASolaraqCharacterPlayerController: Setting up CHARACTER Input Bindings for %s"), *GetName());

    if (CharacterMoveAction) EnhancedInputComponentRef->BindAction(CharacterMoveAction, ETriggerEvent::Triggered, this, &ASolaraqCharacterPlayerController::HandleCharacterMoveInput);

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
        EnhancedInputComponentRef->BindAction(PointerMoveAction, ETriggerEvent::Triggered, this, &ASolaraqCharacterPlayerController::HandlePointerMove);
    }
    if (ToggleFishingModeAction)
    {
        EnhancedInputComponentRef->BindAction(ToggleFishingModeAction, ETriggerEvent::Started, this, &ASolaraqCharacterPlayerController::HandleToggleFishingMode);
    }
    if (SprintAction)
    {
        EnhancedInputComponentRef->BindAction(SprintAction, ETriggerEvent::Started, this, &ASolaraqCharacterPlayerController::HandleSprintStarted);
        EnhancedInputComponentRef->BindAction(SprintAction, ETriggerEvent::Completed, this, &ASolaraqCharacterPlayerController::HandleSprintCompleted);
        UE_LOG(LogSolaraqMovement, Log, TEXT("CharacterPC: Bound SprintAction successfully."));
    }
    else
    {
        UE_LOG(LogSolaraqMovement, Warning, TEXT("CharacterPC: SprintAction is NOT assigned! Sprinting will not work."));
    }
    if (ToggleInventoryAction)
    {
        EnhancedInputComponentRef->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &ASolaraqCharacterPlayerController::HandleCharacterToggleInventory);
        UE_LOG(LogSolaraqSystem, Log, TEXT("CharacterPC: Bound ToggleInventoryAction successfully."));
    }
    else
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("CharacterPC: ToggleInventoryAction is NOT assigned! Inventory will not open."));
    }

    if (ToggleEquipmentWindowAction)
    {
        EnhancedInputComponentRef->BindAction(ToggleEquipmentWindowAction, ETriggerEvent::Started, this, &ASolaraqCharacterPlayerController::HandleToggleEquipmentWindow);
    }
}

void ASolaraqCharacterPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    ASolaraqCharacterPawn* CharPawn = GetControlledCharacter();

    if (CharPawn)
    {
        if (USpringArmComponent* SpringArm = CharPawn->GetSpringArmComponent())
        {
            bool bIsInFishingMode_ThisFrame = false;
            if (UFishingSubsystem* FishingSS = GetWorld()->GetSubsystem<UFishingSubsystem>())
            {
                bIsInFishingMode_ThisFrame = (FishingSS->GetCurrentState() != EFishingState::Idle);
            }

            if (bIsInFishingMode_ThisFrame && !bWasInFishingMode_LastFrame)
            {
                PreFishingZoomLength = TargetZoomLength;
            }
            else if (!bIsInFishingMode_ThisFrame && bWasInFishingMode_LastFrame)
            {
                TargetZoomLength = PreFishingZoomLength;
            }

            if (bIsInFishingMode_ThisFrame)
            {
                TargetZoomLength = FishingModeZoomLength;
                TargetCameraOffset = CharPawn->GetTargetAimingRotation().Vector() * CharPawn->FishingCameraRadius;
                
                CurrentCameraTargetOffset = FVector::ZeroVector;
                bIsInForcedRejoinState = false;
                TimeAtMaxOffset = 0.f;
            }
            else 
            {
                if (bUseCustomCameraLag)
                {
                    const FVector CharacterVelocity = CharPawn->GetVelocity();
                    const FVector VelocityDirection = CharacterVelocity.GetSafeNormal();

                    if (CharacterVelocity.SizeSquared() > 1.f) 
                    {
                        if (FVector::DotProduct(VelocityDirection, LastMovementDirection) < RejoinDirectionChangeThreshold)
                        {
                            bIsInForcedRejoinState = false;
                            TimeAtMaxOffset = 0.0f;
                        }

                        if (bIsInForcedRejoinState)
                        {
                            if (RejoinInterpolationMethod == ERejoinInterpolationType::Linear)
                            {
                                CurrentCameraTargetOffset = FMath::VInterpConstantTo(CurrentCameraTargetOffset, FVector::ZeroVector, DeltaTime, CameraForcedRejoinSpeed_Linear);
                            }
                            else 
                            {
                                CurrentCameraTargetOffset = FMath::VInterpTo(CurrentCameraTargetOffset, FVector::ZeroVector, DeltaTime, CameraForcedRejoinSpeed_Interp);
                            }
                        }
                        else 
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
                    else 
                    {
                        bIsInForcedRejoinState = false;
                        TimeAtMaxOffset = 0.0f;
                        LastMovementDirection = FVector::ZeroVector;
                        CurrentCameraTargetOffset = FMath::VInterpTo(CurrentCameraTargetOffset, FVector::ZeroVector, DeltaTime, CameraRecenteringSpeed);
                    }
                    
                    TargetCameraOffset = CurrentCameraTargetOffset;
                }
                else 
                {
                    TargetCameraOffset = FVector::ZeroVector;
                }
            }

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

    // --- GENERIC AUTO-WALK LOGIC ---
    if (bIsAutoNavigatingToInteract && PendingInteractableActor)
    {
        FVector CurrentLoc = GetControlledCharacter()->GetActorLocation();
        CurrentLoc.Z = PendingInteractionLocation.Z; 
        
        float DistSq = FVector::DistSquared(CurrentLoc, PendingInteractionLocation);
        float RadiusSq = PendingInteractionRadius * PendingInteractionRadius;

        // Check if we arrived
        if (DistSq <= RadiusSq)
        {
            // 1. Stop Moving
            StopMovement();
            bIsAutoNavigatingToInteract = false;

            // 2. Trigger the Interface again
            if (PendingInteractableActor->Implements<UInteractableInterface>())
            {
                UE_LOG(LogSolaraqSystem, Log, TEXT("Auto-Nav complete. Triggering Interact on %s"), *PendingInteractableActor->GetName());
                IInteractableInterface::Execute_Interact(PendingInteractableActor, GetControlledCharacter());
            }

            // 3. Clear pointer
            PendingInteractableActor = nullptr;
        }
        else
        {
            // If we are still moving, we don't spam SimpleMoveToLocation.
            // But we should check if we got stuck.
            if (CharPawn && CharPawn->GetVelocity().SizeSquared() < 1.0f)
            {
                // Optional: We are trying to move but velocity is near zero. 
                // We might be blocked by the object itself (Radius too small) or geometry.
                // Simple workaround: re-issue move occasionally or abort after timeout.
                // For now, we trust SimpleMoveToLocation to navigate around or stop.
                
                // If we are very close but blocked, we might want to just trigger interaction anyway if within a reasonable 'reach' distance (e.g. 200 units)
                if (DistSq < (200.0f * 200.0f))
                {
                     // Force success if we are kinda close but stuck
                     // StopMovement(); 
                     // bIsAutoNavigatingToInteract = false;
                     // ... trigger interact ...
                }
            }
        }
    }
}

void ASolaraqCharacterPlayerController::HandleCharacterMoveInput(const FInputActionValue& Value)
{
    // If player touches WASD, cancel the auto-interaction
    if (bIsAutoNavigatingToInteract)
    {
        UE_LOG(LogSolaraqSystem, Log, TEXT("Auto-Nav cancelled by player input."));
        bIsAutoNavigatingToInteract = false;
        PendingInteractableActor = nullptr;
        StopMovement(); // Ensure navigation path is cleared
    }
    
    if (UFishingSubsystem* FishingSubsystem = GetWorld()->GetSubsystem<UFishingSubsystem>())
    {
        if (FishingSubsystem->GetCurrentState() != EFishingState::Idle)
        {
            UE_LOG(LogSolaraqFishing, Log, TEXT("PC: Movement input detected, cancelling fishing."));
            FishingSubsystem->ResetState();
        }
    }
    
    // Pass input to pawn
    ASolaraqCharacterPawn* CharPawn = GetControlledCharacter();
    if (CharPawn)
    {
        const FVector2D MovementVector = Value.Get<FVector2D>();
        CharPawn->HandleMoveInput(MovementVector);
    }
}

void ASolaraqCharacterPlayerController::HandlePointerMove(const FInputActionValue& Value)
{
    // If we click, we cancel previous auto-nav
    if (bIsAutoNavigatingToInteract)
    {
        bIsAutoNavigatingToInteract = false;
        PendingInteractableActor = nullptr;
    }

    FHitResult Hit;
    if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
    {
        if (Hit.bBlockingHit && Hit.GetActor())
        {
            // 1. Interactable Actor
            if (Hit.GetActor()->Implements<UInteractableInterface>())
            {
                // Trigger Interact. The object determines if we are close enough.
                IInteractableInterface::Execute_Interact(Hit.GetActor(), GetControlledCharacter());
            }
            // 2. Ground
            else
            {
                MoveToDestination(Hit.Location);
            }
        }
    }
}

void ASolaraqCharacterPlayerController::OpenContainerInventory(ASolaraqContainerBase* Container)
{
    if (!Container) return;
    if (!ContainerWindowWidgetClass)
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("OpenContainerInventory FAILED: ContainerWindowWidgetClass is not set in PC Blueprint!"));
        return;
    }
    if (!MainHUDWidgetInstance) return;

    UE_LOG(LogSolaraqSystem, Log, TEXT("PC: Opening Container Inventory for %s"), *Container->GetName());

    // 1. Close existing container window if any
    if (ContainerWindowInstance)
    {
        ContainerWindowInstance->RemoveFromParent();
        ContainerWindowInstance = nullptr;
    }

    // 2. Create the Window
    ContainerWindowInstance = CreateWidget<USolaraqContainerWindowWidget>(this, ContainerWindowWidgetClass);
    if (ContainerWindowInstance)
    {
        // 3. Initialize it with the container actor
        ContainerWindowInstance->InitContainerWindow(Container);
        
        // 4. Bind Close Event so we can null our reference
        ContainerWindowInstance->OnCloseRequested.AddDynamic(this, &ASolaraqCharacterPlayerController::OnContainerClosedByUI);

        // 5. Add to HUD
        MainHUDWidgetInstance->GetMainCanvas()->AddChildToCanvas(ContainerWindowInstance);

        // 6. Position it (Offset to the right side of screen usually)
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(ContainerWindowInstance->Slot))
        {
            CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f)); // Top-Left Anchors for absolute positioning
            CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
            CanvasSlot->SetPosition(FVector2D(900.0f, 100.0f)); // Hardcoded offset for now, adjust based on viewport size later
            CanvasSlot->SetAutoSize(true);
        }

        // 7. Visually Open the Container (Animation)
        Container->OpenContainer(this);

        // 8. Ensure Player Inventory is also open
        if (!CharacterInventoryWidgetInstance)
        {
            HandleCharacterToggleInventory();
        }
        
        // 9. Ensure Mouse is visible and we can click, but DON'T lock it exclusively to UI
        FInputModeGameAndUI InputModeData;
        InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputModeData.SetHideCursorDuringCapture(false);
        SetInputMode(InputModeData);
        SetShowMouseCursor(true);
    }
}

void ASolaraqCharacterPlayerController::OnContainerClosedByUI()
{
    UE_LOG(LogSolaraqSystem, Log, TEXT("PC: Container Window Closed via UI."));
    ContainerWindowInstance = nullptr;
}

// ... (Rest of functions: HandleCharacterInteractInput, HandlePrimaryUse, etc. remain unchanged)
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
        // ... (Velocity check logic remains the same) ...
        if (CharPawn->GetVelocity().SizeSquared() > 1.0f)
        {
            if (UFishingSubsystem* FishingSS = GetWorld()->GetSubsystem<UFishingSubsystem>())
            {
                const EFishingState CurrentFishingState = FishingSS->GetCurrentState();
                if (CurrentFishingState == EFishingState::Idle || CurrentFishingState == EFishingState::ReadyToCast)
                {
                    return; 
                }
            }
        }
    
        UE_LOG(LogSolaraqFishing, Warning, TEXT("PC: HandlePrimaryUseStarted() - Input received."));
    
        // --- CHANGE UEquipmentComponent TO USolaraqEquipmentComponent ---
        if (USolaraqEquipmentComponent* EquipComp = CharPawn->GetEquipmentComponent())
        {
            EquipComp->HandlePrimaryUse(); 
        }
    }
    
}

void ASolaraqCharacterPlayerController::HandlePrimaryUseCompleted()
{
    UE_LOG(LogSolaraqFishing, Warning, TEXT("PC: HandlePrimaryUseCompleted() - Input received."));
    if (ASolaraqCharacterPawn* CharPawn = GetControlledCharacter())
    {
        // --- CHANGE UEquipmentComponent TO USolaraqEquipmentComponent ---
        if (USolaraqEquipmentComponent* EquipComp = CharPawn->GetEquipmentComponent())
        {
            EquipComp->HandlePrimaryUse_Stop(); 
        }
    }
}

void ASolaraqCharacterPlayerController::HandleSecondaryUseStarted()
{
    if (ASolaraqCharacterPawn* CharPawn = GetControlledCharacter())
    {
        // --- CHANGE UEquipmentComponent TO USolaraqEquipmentComponent ---
        if (USolaraqEquipmentComponent* EquipComp = CharPawn->GetEquipmentComponent())
        {
            EquipComp->HandleSecondaryUse(); 
        }
    }
}

void ASolaraqCharacterPlayerController::HandleSecondaryUseCompleted()
{
    if (ASolaraqCharacterPawn* CharPawn = GetControlledCharacter())
    {
        // --- CHANGE UEquipmentComponent TO USolaraqEquipmentComponent ---
        if (USolaraqEquipmentComponent* EquipComp = CharPawn->GetEquipmentComponent())
        {
            EquipComp->HandleSecondaryUse_Stop(); 
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

void ASolaraqCharacterPlayerController::HandleCharacterToggleInventory()
{
    if (CharacterInventoryWidgetInstance)
    {
        // --- CLOSING ---
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CharacterInventoryWidgetInstance->Slot))
        {
            LastInventoryPosition = CanvasSlot->GetPosition();
            bIsInventoryPositionSet = true;
        }

        CharacterInventoryWidgetInstance->RemoveFromParent();
        CharacterInventoryWidgetInstance = nullptr;
    }
    else
    {
        // --- OPENING ---
        if (!CharacterInventoryWidgetClass || !MainHUDWidgetInstance) return;

        CharacterInventoryWidgetInstance = CreateWidget<USolaraqInventoryWindowWidget>(this, CharacterInventoryWidgetClass);
        if (CharacterInventoryWidgetInstance)
        {
            // === FIX: BIND TO THE UI CLOSE EVENT ===
            CharacterInventoryWidgetInstance->OnCloseRequested.AddDynamic(this, &ASolaraqCharacterPlayerController::OnInventoryClosedByUI);
            // =======================================

            MainHUDWidgetInstance->GetMainCanvas()->AddChildToCanvas(CharacterInventoryWidgetInstance);
            
            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CharacterInventoryWidgetInstance->Slot))
            {
                if (bIsInventoryPositionSet)
                {
                    CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f)); // Force Top-Left
                    CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));        // Pivot Top-Left
                    CanvasSlot->SetPosition(LastInventoryPosition);
                }
                else
                {
                    // Default Center
                    CanvasSlot->SetAnchors(FAnchors(0.5f));
                    CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                    CanvasSlot->SetPosition(FVector2D(0, 0));
                }
                CanvasSlot->SetAutoSize(true);
            }

            FInputModeGameAndUI InputModeData;
            InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            InputModeData.SetHideCursorDuringCapture(false);
            SetInputMode(InputModeData);
            SetShowMouseCursor(true); 
        }
    }
}

void ASolaraqCharacterPlayerController::HandleToggleEquipmentWindow()
{
    if (EquipmentWindowInstance)
    {
        // --- CLOSING ---
        // Save Position before destroying
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(EquipmentWindowInstance->Slot))
        {
            LastEquipmentPosition = CanvasSlot->GetPosition();
            bIsEquipmentPositionSet = true;
        }

        EquipmentWindowInstance->RemoveFromParent();
        EquipmentWindowInstance = nullptr;
    }
    else
    {
        // --- OPENING ---
        if (!EquipmentWindowWidgetClass || !MainHUDWidgetInstance) return;

        EquipmentWindowInstance = CreateWidget<USolaraqEquipmentWindowWidget>(this, EquipmentWindowWidgetClass);
        if (EquipmentWindowInstance)
        {
            // Bind the Close Event
            EquipmentWindowInstance->OnCloseRequested.AddDynamic(this, &ASolaraqCharacterPlayerController::OnEquipmentClosedByUI);

            MainHUDWidgetInstance->GetMainCanvas()->AddChildToCanvas(EquipmentWindowInstance);
            
            // Restore Position
            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(EquipmentWindowInstance->Slot))
            {
                if (bIsEquipmentPositionSet)
                {
                    CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f)); // Force Top-Left
                    CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));        // Pivot Top-Left
                    CanvasSlot->SetPosition(LastEquipmentPosition);
                }
                else
                {
                    // Default Offset from Center
                    CanvasSlot->SetAnchors(FAnchors(0.5f));
                    CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                    CanvasSlot->SetPosition(FVector2D(-300, 0)); 
                }
                CanvasSlot->SetAutoSize(true);
            }
        }
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

void ASolaraqCharacterPlayerController::HandleSprintStarted(const FInputActionValue& Value)
{
    UE_LOG(LogSolaraqMovement, Warning, TEXT("CharacterPC %s: HandleSprintStarted CALLED."), *GetNameSafe(this));
    
    ASolaraqCharacterPawn* CharacterPawn = GetControlledCharacter();
    if (CharacterPawn)
    {
        UE_LOG(LogSolaraqMovement, Warning, TEXT("  -> Pawn %s is VALID. Calling StartSprinting()."), *GetNameSafe(CharacterPawn));
        CharacterPawn->StartSprinting();
    }
    else
    {
        UE_LOG(LogSolaraqMovement, Error, TEXT("  -> GetControlledCharacter() is NULL! Cannot start sprinting."), *GetNameSafe(this));
    }
}

void ASolaraqCharacterPlayerController::HandleSprintCompleted(const FInputActionValue& Value)
{
    UE_LOG(LogSolaraqMovement, Warning, TEXT("CharacterPC %s: HandleSprintCompleted CALLED."), *GetNameSafe(this));

    ASolaraqCharacterPawn* CharacterPawn = GetControlledCharacter();
    if (CharacterPawn)
    {
        UE_LOG(LogSolaraqMovement, Warning, TEXT("  -> Pawn %s is VALID. Calling StopSprinting()."), *GetNameSafe(CharacterPawn));
        CharacterPawn->StopSprinting();
    }
    else
    {
        UE_LOG(LogSolaraqMovement, Error, TEXT("  -> GetControlledCharacter() is NULL! Cannot stop sprinting."), *GetNameSafe(this));
    }
}

void ASolaraqCharacterPlayerController::OnInventoryClosedByUI()
{
    HandleCharacterToggleInventory();
}

void ASolaraqCharacterPlayerController::OnEquipmentClosedByUI()
{
    HandleToggleEquipmentWindow();
}