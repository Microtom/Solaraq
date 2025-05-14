// SolaraqPlayerController.cpp

#include "Controllers/SolaraqPlayerController.h"

#include "EngineUtils.h"
#include "EnhancedInputComponent.h" // Include for UEnhancedInputComponent
#include "EnhancedInputSubsystems.h" // Include for Subsystem
#include "InputMappingContext.h" // Include for UInputMappingContext
#include "InputAction.h" // Include for UInputAction
#include "Pawns/SolaraqShipBase.h" // Include ship base class
#include "InputModifiers.h"
#include "GameFramework/Pawn.h"
#include "Logging/SolaraqLogChannels.h"
#include "GenericTeamAgentInterface.h" // Include if needed
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "UI/TargetWidgetInterface.h"


ASolaraqPlayerController::ASolaraqPlayerController()
{
    // Constructor
    TeamId = FGenericTeamId(0); // Ensure Player Team ID is set
    bIsHomingLockActive = false;
    LockedHomingTargetIndex = -1;
    HomingTargetScanRange = 25000.0f; // Sensible default
    HomingTargetScanConeAngleDegrees = 90.0f; // +/- 45 degrees forward
    HomingTargetScanInterval = 0.5f; // Scan 2 times per second
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
                //UE_LOG(LogSolaraqSystem, Log, TEXT("Added Input Mapping Context %s"), *DefaultMappingContext->GetName());
            }
            else
            {
                 //UE_LOG(LogSolaraqSystem, Error, TEXT("%s DefaultMappingContext is not assigned! Input will not work."), *GetName());
            }
        }
         else
             {
                //UE_LOG(LogSolaraqSystem, Error, TEXT("Failed to get EnhancedInputLocalPlayerSubsystem!"));
             }
    }
     else
         {
            //UE_LOG(LogSolaraqSystem, Error, TEXT("Failed to get LocalPlayer!"));
         }
}

void ASolaraqPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Cast InputComponent to UEnhancedInputComponent
    EnhancedInputComponentRef = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInputComponentRef)
    {
        //UE_LOG(LogSolaraqSystem, Error, TEXT("%s Failed to cast InputComponent to UEnhancedInputComponent! Enhanced Input bindings will fail."), *GetName());
        return;
    }

     //UE_LOG(LogSolaraqSystem, Log, TEXT("Setting up Enhanced Input Bindings for %s"), *GetName());

    // --- Bind Actions ---
    // Verify Input Action assets are assigned before binding
    if (MoveAction)
    {
        EnhancedInputComponentRef->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleMoveInput);
         //UE_LOG(LogSolaraqSystem, Verbose, TEXT(" - MoveAction Bound"));
    }
    else
        {
            //UE_LOG(LogSolaraqSystem, Warning, TEXT("MoveAction not assigned in PlayerController!"));
        }

    if (TurnAction)
    {
        EnhancedInputComponentRef->BindAction(TurnAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleTurnInput);
        EnhancedInputComponentRef->BindAction(TurnAction, ETriggerEvent::Completed, this, &ASolaraqPlayerController::HandleTurnCompleted);
        //UE_LOG(LogSolaraqSystem, Verbose, TEXT(" - TurnAction Bound"));
    } else
        {
        //UE_LOG(LogSolaraqSystem, Warning, TEXT("TurnAction not assigned in PlayerController!"));
        }

    if (FireAction)
    {
        // Bind to Triggered for single shots or hold-to-fire start
        EnhancedInputComponentRef->BindAction(FireAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleFireRequest);
         //UE_LOG(LogSolaraqProjectile, Warning, TEXT(" - FireAction Bound"));
    } else
        {
            //UE_LOG(LogSolaraqSystem, Warning, TEXT("FireAction not assigned in PlayerController!"));
        }

    if (BoostAction)
    {
        EnhancedInputComponentRef->BindAction(BoostAction, ETriggerEvent::Started, this, &ASolaraqPlayerController::HandleBoostStarted);
        EnhancedInputComponentRef->BindAction(BoostAction, ETriggerEvent::Completed, this, &ASolaraqPlayerController::HandleBoostCompleted);
         //UE_LOG(LogSolaraqSystem, Verbose, TEXT(" - BoostAction Bound (Started/Completed)"));
    }
    else
    {
        //UE_LOG(LogSolaraqSystem, Warning, TEXT("BoostAction not assigned in PlayerController!"));
    }

    if (FireMissileAction)
    {
        EnhancedInputComponentRef->BindAction(FireMissileAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleFireMissileRequest);
                //UE_LOG(LogSolaraqProjectile, Warning, TEXT(" - FireHomingAction Bound"));
    }
    else
    {
        //UE_LOG(LogSolaraqSystem, Warning, TEXT("FireHomingAction not assigned in PlayerController!"));
    }
    
    if (ToggleLockAction)
    {
        EnhancedInputComponentRef->BindAction(ToggleLockAction, ETriggerEvent::Started, this, &ASolaraqPlayerController::HandleToggleLock);
        UE_LOG(LogSolaraqMarker, Warning, TEXT(" - ToggleLockAction Bound"));
    }
    else
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("ToggleLockAction not assigned!"));
    }
    
    if (SwitchTargetAction)
    {
        // Bind as Axis type, Triggered fires every frame value is non-zero
        EnhancedInputComponentRef->BindAction(SwitchTargetAction, ETriggerEvent::Triggered, this, &ASolaraqPlayerController::HandleSwitchTarget);
        UE_LOG(LogSolaraqMarker, Warning, TEXT(" - SwitchTargetAction Bound"));
    }
    else
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("SwitchTargetAction not assigned!"));
    }
}

void ASolaraqPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Update widget positions every frame if lock is active
    if (bIsHomingLockActive)
    {
        UpdateTargetWidgets(); // Keep widgets positioned correctly
    }
}

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
    //NET_LOG(LogSolaraqGUI, Verbose, TEXT("Cleared all target widgets."));
}

void ASolaraqPlayerController::SelectTargetByIndex(int32 Index)
{
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
        // //UE_LOG(LogSolaraqInput, Verbose, TEXT("HandleTurnCompleted: Sending 0.0 Turn Input"));

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
         // //UE_LOG(LogSolaraqInput, Verbose, TEXT("HandleMoveInput: %.2f"), MoveValue);
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
        // //UE_LOG(LogSolaraqInput, Verbose, TEXT("HandleTurnInput: %.2f"), TurnValue);
    }
}

void ASolaraqPlayerController::HandleFireMissileRequest(const FInputActionValue& Value)
{
    if (!bIsHomingLockActive || !LockedHomingTargetActor.IsValid())
    {
        //UE_LOG(LogSolaraqProjectile, Warning, TEXT("Cannot fire homing missile: Lock not active or no valid target locked."));
        // Optionally play a "failed" sound effect
        return;
    }

    ASolaraqShipBase* Ship = GetControlledShip();
    AActor* Target = LockedHomingTargetActor.Get(); // Get the raw pointer
    
    if (Ship && Target)
    {
        //UE_LOG(LogSolaraqProjectile, Log, TEXT("Requesting homing fire at locked target: %s"), *Target->GetName());
        // Call the MODIFIED server RPC on the ship, passing the target
        Ship->Server_RequestFireHomingMissileAtTarget(Target);
    }
    else
    {
        //UE_LOG(LogSolaraqProjectile, Error, TEXT("Failed fire request: Ship (%s) or Target (%s) invalid."), Ship ? TEXT("OK") : TEXT("NULL"), Target ? TEXT("OK") : TEXT("NULL"));
    }
}

void ASolaraqPlayerController::HandleFireRequest() // Changed parameter list
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        // Call the Server RPC on the Pawn
        Ship->Server_RequestFire();
        // //UE_LOG(LogSolaraqInput, Verbose, TEXT("HandleFireRequest called"));
    }
}

void ASolaraqPlayerController::HandleBoostStarted(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        // Call the Server RPC on the Pawn
        Ship->Server_SetAttemptingBoost(true);
         // //UE_LOG(LogSolaraqInput, Verbose, TEXT("HandleBoostStarted"));
    }
}

void ASolaraqPlayerController::HandleBoostCompleted(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        // Call the Server RPC on the Pawn
        Ship->Server_SetAttemptingBoost(false);
        // //UE_LOG(LogSolaraqInput, Verbose, TEXT("HandleBoostCompleted"));
    }
}

void ASolaraqPlayerController::HandleToggleLock()
{
    bIsHomingLockActive = !bIsHomingLockActive;
    UE_LOG(LogSolaraqMarker, Warning, TEXT("PlayerController: Homing Lock Mode Toggled: %s"), bIsHomingLockActive ? TEXT("ACTIVE") : TEXT("INACTIVE"));

    if (bIsHomingLockActive)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("Homing Lock ACTIVATED. Starting target scan timer."));
        // Start scanning immediately and then periodically
        UpdatePotentialTargets(); // Initial scan
        GetWorldTimerManager().SetTimer(TimerHandle_ScanTargets, this, &ASolaraqPlayerController::UpdatePotentialTargets, HomingTargetScanInterval, true);
    }
    else
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("Homing Lock DEACTIVATED. Clearing scan timer and target data."));
        // Stop scanning and clear state
        GetWorldTimerManager().ClearTimer(TimerHandle_ScanTargets);
        PotentialHomingTargets.Empty();
        LockedHomingTargetIndex = -1;
        LockedHomingTargetActor = nullptr;
        ClearTargetWidgets(); // Remove UI elements
    }
}

void ASolaraqPlayerController::HandleSwitchTarget(const FInputActionValue& Value)
{

    if (!bIsHomingLockActive)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("HandleSwitchTarget(): SwitchTarget ignored: Homing lock not active."));
        return;
    }

    if (PotentialHomingTargets.Num() <= 1)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("SwitchTarget ignored: Not enough potential targets (%d)."), PotentialHomingTargets.Num());
        // Need lock active and more than one target to switch
        return;
    }

    const float SwitchValue = Value.Get<float>(); // Get axis value (e.g., +1.0 or -1.0 from scroll)

    UE_LOG(LogSolaraqMarker, Warning, TEXT("HandleSwitchTarget Called. SwitchValue: %.2f"), SwitchValue);
    
    if (FMath::IsNearlyZero(SwitchValue))
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("SwitchTarget ignored: SwitchValue is nearly zero."));
        // Axis event might fire with zero value sometimes, ignore
        return;
    }

    int32 Direction = FMath::Sign(SwitchValue); // Get +1 or -1
    int32 CurrentIndex = LockedHomingTargetIndex;
    int32 NumTargets = PotentialHomingTargets.Num();

    // Calculate the next index, handling wrap-around
    int32 NextIndex = (CurrentIndex + Direction + NumTargets) % NumTargets; // Modulo for wrap-around

    UE_LOG(LogSolaraqMarker, Warning, TEXT("Switching Target: CurrentIndex: %d, Direction: %d, NumTargets: %d, NextIndex: %d"),
        LockedHomingTargetIndex, // Log original LockedHomingTargetIndex before changing
        Direction,
        NumTargets,
        NextIndex);
    
    SelectTargetByIndex(NextIndex);

    if(LockedHomingTargetActor.IsValid())
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("Successfully Switched Target to: %s (Index: %d)"), *LockedHomingTargetActor->GetName(), LockedHomingTargetIndex);
    }
    else
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("Switched Target, but LockedHomingTargetActor is now invalid (Index: %d). This might be an issue."), LockedHomingTargetIndex);
    }
    
    //UE_LOG(LogSolaraqProjectile, Warning, TEXT("Switched Target: Index %d -> %d"), CurrentIndex, LockedHomingTargetIndex);
}

// --- Optional: Implement GetTeamAttitudeTowards if PlayerController handles team ---
/*
ETeamAttitude::Type ASolaraqPlayerController::GetTeamAttitudeTowards(const AActor& Other) const
{
    // ... (Implementation similar to previous examples) ...
}
*/