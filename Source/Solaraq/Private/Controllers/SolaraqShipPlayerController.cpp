// SolaraqShipPlayerController.cpp

#include "Controllers/SolaraqShipPlayerController.h" // Adjust to your path
#include "Pawns/SolaraqShipBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Blueprint/UserWidget.h" // For target markers
#include "UI/TargetWidgetInterface.h" // For target markers
#include "Kismet/GameplayStatics.h" // For ProjectWorldToScreen
#include "EngineUtils.h" // For TActorIterator
#include "Logging/SolaraqLogChannels.h"

ASolaraqShipPlayerController::ASolaraqShipPlayerController()
{
    // Initialize Homing Lock System variables (moved from old PC constructor)
    bIsHomingLockActive = false;
    LockedHomingTargetIndex = -1;
    HomingTargetScanRange = 25000.0f;
    HomingTargetScanConeAngleDegrees = 90.0f;
    HomingTargetScanInterval = 0.5f;
}

void ASolaraqShipPlayerController::RequestTransitionToCharacterLevel(FName TargetLevelName, FName DockingPadID,
    ASolaraqShipBase* FromShip)
{
    if (!FromShip)
    {
        UE_LOG(LogSolaraqTransition, Error, TEXT("ShipPC %s: RequestTransitionToCharacterLevel called with no FromShip!"), *GetNameSafe(this));
        return;
    }
    UE_LOG(LogSolaraqTransition, Log, TEXT("ShipPC %s: Requesting transition to character level '%s' from ship '%s' at pad '%s'."),
        *GetNameSafe(this), *TargetLevelName.ToString(), *FromShip->GetName(), *DockingPadID.ToString());

    // Call the Server RPC
    Server_ExecuteTransitionToCharacterLevel(TargetLevelName, DockingPadID, FromShip);
}

ASolaraqShipBase* ASolaraqShipPlayerController::GetControlledShip() const
{
    return Cast<ASolaraqShipBase>(GetPawn());
}

void ASolaraqShipPlayerController::Server_ExecuteTransitionToCharacterLevel_Implementation(FName TargetLevelName,
    FName DockingPadID, ASolaraqShipBase* FromShip)
{
    UE_LOG(LogSolaraqTransition, Log, TEXT("ShipPC %s (SERVER): Executing transition to character level '%s' from ship '%s' at pad '%s'."),
        *GetNameSafe(this), *TargetLevelName.ToString(), *FromShip->GetName(), *DockingPadID.ToString());

    // Now call the base class's authoritative function to handle GI prep and ClientTravel
    Super::Server_InitiateSeamlessTravelToLevel(TargetLevelName, true /*bIsCharacterLevel*/, DockingPadID, FromShip);
}

bool ASolaraqShipPlayerController::Server_ExecuteTransitionToCharacterLevel_Validate(FName TargetLevelName,
    FName DockingPadID, ASolaraqShipBase* FromShip)
{
    // Add validation: Is TargetLevelName valid? Is FromShip not null and a valid ship controlled by this PC?
    // For now, basic validation.
    if (!FromShip || FromShip->GetController() != this) // Basic check: is this my ship?
    {
        UE_LOG(LogSolaraqTransition, Warning, TEXT("ShipPC %s: Server_ExecuteTransitionToCharacterLevel_Validate failed. FromShip is null or not controlled by this PC."), *GetNameSafe(this));
        return false;
    }
    return true;
}

void ASolaraqShipPlayerController::ApplyShipInputMappingContext()
{
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            // Clear any existing contexts from other controller types (if any were added by mistake or during a complex transition)
            // This might be overly aggressive if base controller adds contexts, but for now, assume this PC owns its context exclusively.
            ClearAllInputContexts(InputSubsystem); // Call base helper to clear all

            if (IMC_ShipControls)
            {
                AddInputContext(InputSubsystem, IMC_ShipControls, 0); // Call base helper
                UE_LOG(LogSolaraqSystem, Log, TEXT("ASolaraqShipPlayerController: Applied SHIP Input Mapping Context: %s"), *IMC_ShipControls->GetName());
            }
            else
            {
                UE_LOG(LogSolaraqSystem, Error, TEXT("ASolaraqShipPlayerController: IMC_ShipControls is not assigned! Ship input will not work."));
            }
        }
    }
}

void ASolaraqShipPlayerController::BeginPlay()
{
    Super::BeginPlay();
    // OnPossess will likely handle the initial context application if a pawn is possessed at BeginPlay.
    // But it's safe to call here too, especially for scenarios where OnPossess might not fire immediately
    // or if starting without a pawn initially.
    if (GetPawn()) // Only apply if we already possess a pawn
    {
       ApplyShipInputMappingContext();
    }
}

void ASolaraqShipPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn); // Calls ASolaraqBasePlayerController::OnPossess

    ASolaraqShipBase* PossessedShip = Cast<ASolaraqShipBase>(InPawn);
    FString AuthorityPrefix = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");

    if (PossessedShip)
    {
        UE_LOG(LogSolaraqMovement, Warning, TEXT("%s ASolaraqShipPlayerController (%s): OnPossess - Possessing SHIP: %s"),
            *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(PossessedShip));
        ApplyShipInputMappingContext();
    }
    else
    {
        UE_LOG(LogSolaraqMovement, Error, TEXT("%s ASolaraqShipPlayerController (%s): OnPossess - FAILED to cast InPawn (%s) to ASolaraqShipBase."),
            *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(InPawn));
        // Optionally clear contexts if possessing something unexpected
        if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                ClearAllInputContexts(InputSubsystem);
            }
        }
    }
}

void ASolaraqShipPlayerController::OnUnPossess()
{
    FString AuthorityPrefix = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
    UE_LOG(LogSolaraqMovement, Log, TEXT("%s ASolaraqShipPlayerController (%s): OnUnPossess - Unpossessing: %s."),
        *AuthorityPrefix, *GetNameSafe(this), *GetNameSafe(GetPawn()));

    // Clear ship-specific input context
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if (IMC_ShipControls)
            {
                InputSubsystem->RemoveMappingContext(IMC_ShipControls);
            }
        }
    }
    // Clear homing lock state when unpossessing ship
    if (bIsHomingLockActive)
    {
        bIsHomingLockActive = false;
        GetWorldTimerManager().ClearTimer(TimerHandle_ScanTargets);
        PotentialHomingTargets.Empty();
        LockedHomingTargetIndex = -1;
        LockedHomingTargetActor = nullptr;
        ClearTargetWidgets();
    }

    Super::OnUnPossess(); // Calls ASolaraqBasePlayerController::OnUnPossess
}

void ASolaraqShipPlayerController::OnRep_Pawn()
{
    Super::OnRep_Pawn(); // Important: Let base handle its logic.
                         // Base OnRep_Pawn doesn't do much yet, but good practice.

    // This is called on the CLIENT when the pawn replicates.
    // Ensure the correct input context is applied.
    // OnPossess should have been called by the engine's replication system if the pawn changed significantly,
    // but this is a good safeguard.
    ApplyShipInputMappingContext();
}


void ASolaraqShipPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent(); // This calls base and gets EnhancedInputComponentRef

    if (!EnhancedInputComponentRef) // Guard against base failing
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("ASolaraqShipPlayerController: EnhancedInputComponentRef is NULL after Super::SetupInputComponent! Cannot bind ship actions."));
        return;
    }

    UE_LOG(LogSolaraqSystem, Log, TEXT("ASolaraqShipPlayerController: Setting up SHIP Input Bindings for %s"), *GetName());

    if (MoveAction) EnhancedInputComponentRef->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASolaraqShipPlayerController::HandleMoveInput);
    if (TurnAction)
    {
        EnhancedInputComponentRef->BindAction(TurnAction, ETriggerEvent::Triggered, this, &ASolaraqShipPlayerController::HandleTurnInput);
        EnhancedInputComponentRef->BindAction(TurnAction, ETriggerEvent::Completed, this, &ASolaraqShipPlayerController::HandleTurnCompleted);
    }
    if (FireAction) EnhancedInputComponentRef->BindAction(FireAction, ETriggerEvent::Triggered, this, &ASolaraqShipPlayerController::HandleFireRequest);
    if (BoostAction)
    {
        EnhancedInputComponentRef->BindAction(BoostAction, ETriggerEvent::Started, this, &ASolaraqShipPlayerController::HandleBoostStarted);
        EnhancedInputComponentRef->BindAction(BoostAction, ETriggerEvent::Completed, this, &ASolaraqShipPlayerController::HandleBoostCompleted);
    }
    if (FireMissileAction) EnhancedInputComponentRef->BindAction(FireMissileAction, ETriggerEvent::Triggered, this, &ASolaraqShipPlayerController::HandleFireMissileRequest);
    if (ToggleLockAction) EnhancedInputComponentRef->BindAction(ToggleLockAction, ETriggerEvent::Started, this, &ASolaraqShipPlayerController::HandleToggleLock);
    if (SwitchTargetAction) EnhancedInputComponentRef->BindAction(SwitchTargetAction, ETriggerEvent::Triggered, this, &ASolaraqShipPlayerController::HandleSwitchTarget);
    if (ToggleShieldAction) {
        EnhancedInputComponentRef->BindAction(ToggleShieldAction, ETriggerEvent::Started, this, &ASolaraqShipPlayerController::HandleToggleShieldInput);
        UE_LOG(LogSolaraqShield, Warning, TEXT("ShipPC %s: SetupInputComponent - Bound ToggleShieldAction to HandleToggleShieldInput."), *GetNameSafe(this));
    } else {
        UE_LOG(LogSolaraqShield, Warning, TEXT("ShipPC %s: SetupInputComponent - ToggleShieldAction IS NULL! Cannot bind shield toggle."), *GetNameSafe(this));
    }
    // InteractAction is defined in ASolaraqBasePlayerController
    // Bind it here for ship-specific interaction
    if (InteractAction) { 
        EnhancedInputComponentRef->BindAction(InteractAction, ETriggerEvent::Started, this, &ASolaraqShipPlayerController::HandleShipInteractInput);
        UE_LOG(LogSolaraqTransition, Warning, TEXT("ASolaraqShipPlayerController %s: SetupInputComponent - SUCCESSFULLY BOUND InteractAction to HandleShipInteractInput."), *GetNameSafe(this));
    } else {
        UE_LOG(LogSolaraqTransition, Error, TEXT("ASolaraqShipPlayerController %s: SetupInputComponent - InteractAction IS NULL! Cannot bind HandleShipInteractInput."), *GetNameSafe(this));
    }
}

void ASolaraqShipPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Update widget positions every frame if lock is active
    if (bIsHomingLockActive) // No need to check control mode, this PC IS the ship controller
    {
        UpdateTargetWidgets();
    }
}

// --- Ship Input Handlers (Copied from ASolaraqPlayerController.cpp, GetControlledShip() usage is now direct) ---
void ASolaraqShipPlayerController::HandleMoveInput(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        const float MoveValue = Value.Get<float>();
        if (GetNetMode() == NM_Client) {
            UE_LOG(LogSolaraqMovement, Warning, TEXT("CLIENT ShipPC %s: Attempting to call Server_SendMoveForwardInput on Ship %s with Value: %.2f"),
                *GetNameSafe(this), *GetNameSafe(Ship), MoveValue);
        }
        Ship->Server_SendMoveForwardInput(MoveValue);
    }
    else if (GetNetMode() == NM_Client)
    {
        UE_LOG(LogSolaraqMovement, Error, TEXT("CLIENT ShipPC %s: HandleMoveInput: GetControlledShip() is NULL! Cannot send RPC."), *GetNameSafe(this));
    }
}

void ASolaraqShipPlayerController::HandleTurnInput(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        const float TurnValue = Value.Get<float>();
        Ship->Server_SendTurnInput(TurnValue);
    }
}

void ASolaraqShipPlayerController::HandleTurnCompleted(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        Ship->Server_SendTurnInput(0.0f);
    }
}

void ASolaraqShipPlayerController::HandleFireRequest()
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        Ship->Server_RequestFire();
    }
}

void ASolaraqShipPlayerController::HandleBoostStarted(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        Ship->Server_SetAttemptingBoost(true);
    }
}

void ASolaraqShipPlayerController::HandleBoostCompleted(const FInputActionValue& Value)
{
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        Ship->Server_SetAttemptingBoost(false);
    }
}

void ASolaraqShipPlayerController::HandleFireMissileRequest(const FInputActionValue& Value)
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
}

void ASolaraqShipPlayerController::HandleShipInteractInput()
{
    UE_LOG(LogSolaraqTransition, Warning, TEXT("ShipPC %s: HandleShipInteractInput called."), *GetNameSafe(this));
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship && Ship->IsShipDocked())
    {
        // The ship's RequestInteraction will call ASolaraqBasePlayerController::InitiateLevelTransitionToCharacter
        Ship->RequestInteraction(); 
        UE_LOG(LogSolaraqTransition, Warning, TEXT("ShipPC: Sent Interact request to docked ship %s."), *Ship->GetName());
    }
    else if (Ship)
    {
        UE_LOG(LogSolaraqTransition, Warning, TEXT("ShipPC: Interact pressed, but ship %s is not docked."), *Ship->GetName());
    }
    else
    {
        UE_LOG(LogSolaraqTransition, Warning, TEXT("ShipPC: Interact pressed, but GetControlledShip() is NULL."));
    }
}

void ASolaraqShipPlayerController::HandleToggleShieldInput()
{
    UE_LOG(LogSolaraqShield, Warning, TEXT("ShipPC %s: HandleToggleShieldInput CALLED."), *GetNameSafe(this));
    ASolaraqShipBase* Ship = GetControlledShip();
    if (Ship)
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("ShipPC %s: Controlled ship %s is VALID. Requesting shield toggle via Ship->Server_RequestToggleShield()."), 
            *GetNameSafe(this), *GetNameSafe(Ship));
        Ship->Server_RequestToggleShield();
    }
    else
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("ShipPC %s: HandleToggleShieldInput called, but GetControlledShip() is NULL. Cannot request shield toggle."), *GetNameSafe(this));
    }
}

// --- Homing Lock System Functions (Copied from ASolaraqPlayerController.cpp) ---
// (No CurrentControlMode checks needed within these functions anymore)
void ASolaraqShipPlayerController::HandleToggleLock()
{
    // Action only valid in ship mode - this IS ship mode.
    bIsHomingLockActive = !bIsHomingLockActive;
    UE_LOG(LogSolaraqMarker, Warning, TEXT("ShipPC: Homing Lock Mode Toggled: %s"), bIsHomingLockActive ? TEXT("ACTIVE") : TEXT("INACTIVE"));

    if (bIsHomingLockActive)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("ShipPC: Homing Lock ACTIVATED. Starting target scan timer."));
        UpdatePotentialTargets(); // Initial scan
        GetWorldTimerManager().SetTimer(TimerHandle_ScanTargets, this, &ASolaraqShipPlayerController::UpdatePotentialTargets, HomingTargetScanInterval, true);
    }
    else
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("ShipPC: Homing Lock DEACTIVATED. Clearing scan timer and target data."));
        GetWorldTimerManager().ClearTimer(TimerHandle_ScanTargets);
        PotentialHomingTargets.Empty();
        LockedHomingTargetIndex = -1;
        LockedHomingTargetActor = nullptr;
        ClearTargetWidgets();
    }
}

void ASolaraqShipPlayerController::HandleSwitchTarget(const FInputActionValue& Value)
{
    if (!bIsHomingLockActive)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("ShipPC: SwitchTarget ignored: Homing lock not active."));
        return;
    }
    if (PotentialHomingTargets.Num() <= 1)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("ShipPC: SwitchTarget ignored: Not enough potential targets (%d)."), PotentialHomingTargets.Num());
        return;
    }

    const float SwitchValue = Value.Get<float>();
    if (FMath::IsNearlyZero(SwitchValue))
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("ShipPC: SwitchTarget ignored: SwitchValue is nearly zero."));
        return;
    }

    int32 Direction = FMath::Sign(SwitchValue);
    int32 CurrentIdx = LockedHomingTargetIndex;
    int32 NumTargets = PotentialHomingTargets.Num();
    int32 NextIndex = (CurrentIdx + Direction + NumTargets) % NumTargets;

    UE_LOG(LogSolaraqMarker, Warning, TEXT("ShipPC: Switching Target: CurrentIndex: %d, Direction: %d, NumTargets: %d, NextIndex: %d"),
        CurrentIdx, Direction, NumTargets, NextIndex);
    
    SelectTargetByIndex(NextIndex);

    if(LockedHomingTargetActor.IsValid())
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("ShipPC: Successfully Switched Target to: %s (Index: %d)"), *LockedHomingTargetActor->GetName(), LockedHomingTargetIndex);
    }
}

void ASolaraqShipPlayerController::UpdatePotentialTargets()
{
    // Copied from ASolaraqPlayerController::UpdatePotentialTargets
    // Ensure MyTeamAgent is correctly determined from GetControlledShip() or 'this' (the controller)
    // Remove CurrentControlMode checks
    UE_LOG(LogSolaraqMarker, Warning, TEXT("--- ShipPC: Begin UpdatePotentialTargets ---"));
    
    if (!bIsHomingLockActive)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("ShipPC: UpdatePotentialTargets: Homing lock not active. Aborting scan."));
        return;
    }
    
    ASolaraqShipBase* MyShip = GetControlledShip();
    if (!MyShip)
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("ShipPC: UpdatePotentialTargets: Controlled ship is NULL. Aborting scan."));
        return;
    }

    UE_LOG(LogSolaraqMarker, Warning, TEXT("ShipPC: UpdatePotentialTargets: Scanning for player %s (Ship: %s)"), *GetName(), *MyShip->GetName());
    
    // IGenericTeamAgentInterface* MyTeamAgent = Cast<IGenericTeamAgentInterface>(MyShip->GetController()); // Controller is 'this'
    IGenericTeamAgentInterface* MyTeamAgent = this; // Controller implements it

    if (!MyTeamAgent) // Should not happen if this class implements it.
    {
        UE_LOG(LogSolaraqMarker, Warning, TEXT("ShipPC: This controller does not implement IGenericTeamAgentInterface. Cannot determine attitude."));
        return;
    }
    
    TArray<TWeakObjectPtr<AActor>> NewPotentialTargets;
    const FVector SelfLocation = MyShip->GetActorLocation();
    const FVector SelfForward = MyShip->GetActorForwardVector();
    const float MaxRangeSq = FMath::Square(HomingTargetScanRange);
    const float MinDotProduct = FMath::Cos(FMath::DegreesToRadians(HomingTargetScanConeAngleDegrees / 2.0f));
    
    int32 IteratedActorsCount = 0;
    for (TActorIterator<ASolaraqShipBase> It(GetWorld()); It; ++It)
    {
        IteratedActorsCount++;
        ASolaraqShipBase* PotentialTargetShip = *It;
        if (!PotentialTargetShip || PotentialTargetShip == MyShip || PotentialTargetShip->IsDead())
        {
            continue;
        }

        IGenericTeamAgentInterface* OtherTeamAgent = Cast<IGenericTeamAgentInterface>(PotentialTargetShip->GetController());
        if (!OtherTeamAgent) OtherTeamAgent = Cast<IGenericTeamAgentInterface>(PotentialTargetShip); // Fallback to pawn if it implements it

        if (!OtherTeamAgent) continue;
        
        ETeamAttitude::Type Attitude = FGenericTeamId::GetAttitude(MyTeamAgent->GetGenericTeamId(), OtherTeamAgent->GetGenericTeamId());
        if (Attitude != ETeamAttitude::Hostile)
        {
            continue;
        }
        
        const FVector TargetLocation = PotentialTargetShip->GetActorLocation();
        const float DistSq = FVector::DistSquared(SelfLocation, TargetLocation);

        if (DistSq < MaxRangeSq)
        {
            const FVector DirectionToTarget = (TargetLocation - SelfLocation).GetSafeNormal();
            const float DotToTarget = FVector::DotProduct(SelfForward, DirectionToTarget);
            
            if (DotToTarget >= MinDotProduct)
            {
                NewPotentialTargets.Add(PotentialTargetShip);
            }
        }
    }
    
    AActor* PreviouslyLockedActor = LockedHomingTargetActor.Get();
    PotentialHomingTargets = NewPotentialTargets;

    int32 FoundPreviousLockIndex = -1;
    if (PreviouslyLockedActor)
    {
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
        SelectTargetByIndex(FoundPreviousLockIndex);
    }
    else if (PotentialHomingTargets.Num() > 0)
    {
        SelectTargetByIndex(0);
    }
    else
    {
        SelectTargetByIndex(-1);
    }
}

void ASolaraqShipPlayerController::UpdateTargetWidgets()
{
    // Copied from ASolaraqPlayerController::UpdateTargetWidgets
    // Remove CurrentControlMode checks
    if (!IsLocalController() || !TargetMarkerWidgetClass) return;

    FVector2D ScreenSize;
    if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(ScreenSize);
    else return;

    TSet<TWeakObjectPtr<AActor>> CurrentTargetsOnScreen;

    for (int32 i = 0; i < PotentialHomingTargets.Num(); ++i)
    {
        TWeakObjectPtr<AActor> TargetPtr = PotentialHomingTargets[i];
        AActor* TargetActor = TargetPtr.Get();
        if (!TargetActor) continue;

        FVector2D ScreenPosition;
        if (UGameplayStatics::ProjectWorldToScreen(this, TargetActor->GetActorLocation(), ScreenPosition, false))
        {
            if (ScreenPosition.X >= 0 && ScreenPosition.X <= ScreenSize.X && ScreenPosition.Y >= 0 && ScreenPosition.Y <= ScreenSize.Y)
            {
                CurrentTargetsOnScreen.Add(TargetPtr);
                TObjectPtr<UUserWidget>* FoundWidgetPtr = TargetMarkerWidgets.Find(TargetPtr);
                UUserWidget* WidgetInstance = (FoundWidgetPtr && *FoundWidgetPtr) ? (*FoundWidgetPtr).Get() : nullptr;

                if (!WidgetInstance)
                {
                    WidgetInstance = CreateWidget<UUserWidget>(this, TargetMarkerWidgetClass);
                    if (WidgetInstance)
                    {
                        WidgetInstance->AddToViewport();
                        TargetMarkerWidgets.Add(TargetPtr, WidgetInstance);
                    }
                    else continue;
                }
                WidgetInstance->SetPositionInViewport(ScreenPosition, true);
                WidgetInstance->SetVisibility(LockedHomingTargetActor.Get() == TargetActor ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
                
                // Optional: Interface call to set locked state for visual changes
                if (ITargetWidgetInterface* TargetWidget = Cast<ITargetWidgetInterface>(WidgetInstance))
                {
                    TargetWidget->Execute_SetLockedState(WidgetInstance, (LockedHomingTargetActor.Get() == TargetActor));
                }
            }
            else // Off-screen
            {
                if (TObjectPtr<UUserWidget>* FoundWidgetPtr = TargetMarkerWidgets.Find(TargetPtr))
                    if (UUserWidget* Widget = FoundWidgetPtr->Get()) Widget->SetVisibility(ESlateVisibility::Hidden);
            }
        }
        else // Projection failed
        {
             if (TObjectPtr<UUserWidget>* FoundWidgetPtr = TargetMarkerWidgets.Find(TargetPtr))
                if (UUserWidget* Widget = FoundWidgetPtr->Get()) Widget->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    TArray<TWeakObjectPtr<AActor>> WidgetsToRemoveKeys;
    for (auto It = TargetMarkerWidgets.CreateIterator(); It; ++It)
    {
        if (!It.Value() || !PotentialHomingTargets.Contains(It.Key()) || !CurrentTargetsOnScreen.Contains(It.Key()))
        {
            if (It.Value()) It.Value()->RemoveFromParent();
            WidgetsToRemoveKeys.Add(It.Key());
        }
    }
    for (const auto& KeyToRemove : WidgetsToRemoveKeys) TargetMarkerWidgets.Remove(KeyToRemove);
}

void ASolaraqShipPlayerController::ClearTargetWidgets()
{
    // Copied from ASolaraqPlayerController::ClearTargetWidgets
    for (auto const& [TargetPtr, WidgetInstance] : TargetMarkerWidgets)
    {
        if (WidgetInstance) WidgetInstance->RemoveFromParent();
    }
    TargetMarkerWidgets.Empty();
}

void ASolaraqShipPlayerController::SelectTargetByIndex(int32 Index)
{
    // Copied from ASolaraqPlayerController::SelectTargetByIndex
    // Remove CurrentControlMode checks
    AActor* PreviousLock = LockedHomingTargetActor.Get();

    if (Index >= 0 && Index < PotentialHomingTargets.Num() && PotentialHomingTargets[Index].IsValid())
    {
        LockedHomingTargetActor = PotentialHomingTargets[Index];
        LockedHomingTargetIndex = Index;
    }
    else
    {
        LockedHomingTargetActor = nullptr;
        LockedHomingTargetIndex = -1;
    }

    // Update widget for previous lock (if it changed and was valid)
    if (PreviousLock && PreviousLock != LockedHomingTargetActor.Get())
    {
        if (TObjectPtr<UUserWidget>* FoundWidget = TargetMarkerWidgets.Find(PreviousLock))
            if (UUserWidget* Widget = FoundWidget->Get())
                 if (ITargetWidgetInterface* TargetWidget = Cast<ITargetWidgetInterface>(Widget))
                    TargetWidget->Execute_SetLockedState(Widget, false);
    }
    // Update widget for new lock (if valid)
    if (LockedHomingTargetActor.IsValid())
    {
         if (TObjectPtr<UUserWidget>* FoundWidget = TargetMarkerWidgets.Find(LockedHomingTargetActor))
            if (UUserWidget* Widget = FoundWidget->Get())
                 if (ITargetWidgetInterface* TargetWidget = Cast<ITargetWidgetInterface>(Widget))
                    TargetWidget->Execute_SetLockedState(Widget, true);
    }
     // UpdateTargetWidgets in Tick will handle visibility.
}