// FishingSubsystem.cpp
#include "Systems/FishingSubsystem.h"

#include "Controllers/SolaraqCharacterPlayerController.h"
#include "Items/ItemDataAssetBase.h"
#include "Items/Fishing/FishDataTable.h"
#include "Pawns/SolaraqCharacterPawn.h"
#include "Items/Fishing/ItemActor_FishingRod.h" 
#include "Items/Fishing/FishingBobber.h"  
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Logging/SolaraqLogChannels.h"

void UFishingSubsystem::Tick(float DeltaTime)
{
    if (CurrentState == EFishingState::Casting)
    {
        CastCharge = FMath::Clamp(CastCharge + DeltaTime, 0.f, 1.f);
    }
    
    // Check if reeling is finished
    if (CurrentState == EFishingState::Reeling)
    {
        UpdateTension(DeltaTime);

        // A simple condition to check if the fish is "caught"
        if (ActiveRod && ActiveRod->CurrentRopeLength <= ActiveRod->RopeSegmentLength * 2.0f)
        {
            CatchFish();
        }
    }

    if (CurrentState != EFishingState::Idle && CurrentFisher)
    {
        // Use a small threshold to avoid cancelling due to tiny animation drifts
        if (CurrentFisher->GetVelocity().SizeSquared() > 1.0f)
        {
            UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: Pawn is moving, resetting fishing state."));
            ResetState();
        }
    }
}

void UFishingSubsystem::EnterFishingStance(ASolaraqCharacterPawn* Requester)
{
    if (!Requester) return;

    UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: Entering fishing stance for %s."), *Requester->GetName());

    Requester->SetContinuousAiming(true);

    CurrentState = EFishingState::ReadyToCast;
    CurrentFisher = Requester;
}

void UFishingSubsystem::UpdateTension(float DeltaTime)
{
    if (!ActiveRod) return;

    // Is the player actively reeling? (Holding the button)
    const bool bIsPlayerReeling = ActiveRod->IsReeling();

    // 1. Tension increases if the player is reeling.
    if (bIsPlayerReeling)
    {
        CurrentLineTension += TensionIncreaseRate * DeltaTime;
    }
    // 2. Tension increases SIGNIFICANTLY if the fish is pulling.
    if (bIsFishPulling)
    {
        CurrentLineTension += FishPullTensionRate * DeltaTime;
    }
    // 3. Tension decreases if the player is NOT reeling and the fish is NOT pulling.
    if (!bIsPlayerReeling && !bIsFishPulling)
    {
        CurrentLineTension -= TensionDecreaseRate * DeltaTime;
    }
    
    // Clamp the value between 0 and max.
    CurrentLineTension = FMath::Clamp(CurrentLineTension, 0.f, MaxLineTension);

    // 4. Check for failure!
    if (CurrentLineTension >= MaxLineTension)
    {
        OnLineSnap();
        return; // Stop further processing
    }

    // This is where you'd update a UI element to show the tension meter.
    // For now, let's log it.
    UE_LOG(LogSolaraqFishing, Log, TEXT("Tension: %.2f | PlayerReeling: %d | FishPulling: %d"),
        CurrentLineTension, bIsPlayerReeling, bIsFishPulling);
}

void UFishingSubsystem::StartFishBehavior()
{
    bIsFishPulling = true; // Start by pulling
    float InitialDelay = FMath::RandRange(0.5f, 1.5f); // Time until first behavior change
    GetWorld()->GetTimerManager().SetTimer(FishBehaviorTimerHandle, this, &UFishingSubsystem::ToggleFishBehavior, InitialDelay, true);
}

void UFishingSubsystem::ToggleFishBehavior()
{
    bIsFishPulling = !bIsFishPulling;

    // Set next toggle time. Fish could pull for short bursts (1-2s) 
    // and rest for longer periods (2-4s).
    float NextToggleTime = bIsFishPulling ? FMath::RandRange(1.0f, 2.0f) : FMath::RandRange(2.0f, 4.0f);
    GetWorld()->GetTimerManager().SetTimer(FishBehaviorTimerHandle, this, &UFishingSubsystem::ToggleFishBehavior, NextToggleTime, false);
}

void UFishingSubsystem::OnLineSnap()
{
    UE_LOG(LogSolaraqFishing, Warning, TEXT("LINE SNAPPED!"));
    // Here you would play a sound effect, show a message.
    ResetState();
}

void UFishingSubsystem::RequestPrimaryAction(ASolaraqCharacterPawn* Caster, AItemActor_FishingRod* Rod)
{
    UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: RequestPrimaryAction. Current State: %s"), *UEnum::GetValueAsString(CurrentState));
    switch (CurrentState)
    {
    case EFishingState::ReadyToCast:
        // Same as before
            CurrentState = EFishingState::Casting;
        CurrentFisher = Caster;
        ActiveRod = Rod;
        CastCharge = 0.f;
        break;

        // --- THIS IS THE KEY CHANGE ---
    case EFishingState::Idle:
        // If we are idle, first enter the stance...
            EnterFishingStance(Caster);
        // ...and then immediately transition to casting.
        CurrentState = EFishingState::Casting;
        ActiveRod = Rod;
        CastCharge = 0.f;
        UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: Primary Action from Idle triggered stance and casting."));
        break;
        // ----------------------------

    case EFishingState::Fishing:
    case EFishingState::FishHooked:
        GetWorld()->GetTimerManager().ClearTimer(HookedTimerHandle);
        CurrentState = EFishingState::Reeling;
        GetWorld()->GetTimerManager().ClearTimer(FishBiteTimerHandle);
        if (ActiveRod)
        {
            ActiveRod->StartReeling();
        }
        // NEW: Show the HUD
        if (ASolaraqCharacterPlayerController* PC = Cast<ASolaraqCharacterPlayerController>(CurrentFisher->GetController()))
        {
            PC->ShowFishingHUD();
        }
        StartFishBehavior();
        break;

    default:
        break;
    }
}

void UFishingSubsystem::RequestPrimaryAction_Stop(ASolaraqCharacterPawn* Caster, AItemActor_FishingRod* Rod)
{
    UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: RequestPrimaryAction_Stop. Current State: %s"), *UEnum::GetValueAsString(CurrentState));

    // --- Handle releasing a cast (Unchanged) ---
    if (CurrentState == EFishingState::Casting && Caster == CurrentFisher)
    {
        // When we cast the line, we should stop continuously aiming.
        Caster->SetContinuousAiming(false); // NEW

        const FVector AimDirection = Caster->GetAimDirection();
        Rod->SpawnAndCastBobber(AimDirection, CastCharge);
    
        CurrentState = EFishingState::Fishing;
        UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: Cast released. New state: Fishing. Waiting for BOBBER TO LAND."));

        return; 
    }
    
    // --- NEW: Handle stopping the reel ---
    if (CurrentState == EFishingState::Reeling && Caster == CurrentFisher)
    {
        UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: Reeling stopped. Returning to Fishing state."));

        if (ActiveRod)
        {
            ActiveRod->StopReeling();
        }

        // We stopped reeling, but the line is still out. Go back to waiting for a bite.
        CurrentState = EFishingState::Fishing;
        StartFishingSequence();
        return; // Exit after handling
    }
}

void UFishingSubsystem::OnToolUnequipped(AItemActor_FishingRod* Rod)
{
    // If the tool being unequipped is the one we are actively using, reset everything.
    if (Rod == ActiveRod)
    {
        ResetState();
    }
}

void UFishingSubsystem::CatchFish()
{
    if (CurrentState != EFishingState::Reeling || !CurrentFisher)
    {
        return;
    }

    // --- NEW LOOT LOGIC ---
    if (FishLootTable)
    {
        // Get all rows from the table
        TArray<FName> RowNames = FishLootTable->GetRowNames();
        if (RowNames.Num() > 0)
        {
            // Pick a random row
            // (A weighted random would be better, but this is a simple start)
            const FName RandomRowName = RowNames[FMath::RandRange(0, RowNames.Num() - 1)];
            
            // Look up the data in that row
            static const FString ContextString(TEXT("FishingLootContext"));
            FFishLootEntry* LootEntry = FishLootTable->FindRow<FFishLootEntry>(RandomRowName, ContextString);

            if (LootEntry && LootEntry->FishItemData)
            {
                if (UInventoryComponent* Inventory = CurrentFisher->GetInventoryComponent())
                {
                    UE_LOG(LogTemp, Warning, TEXT("Caught a %s!"), *LootEntry->FishItemData->DisplayName.ToString());
                    Inventory->AddItem(LootEntry->FishItemData, 1);
                }
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("FishingSubsystem: FishLootTable is not set in Project Settings!"));
    }

    ResetState();
}

void UFishingSubsystem::RequestToggleFishingMode(ASolaraqCharacterPawn* Requester)
{
    // TODO Must ensure that a fishing rod is equipped. Possibly message the player.
    
    if (!Requester) return;

    if (CurrentState == EFishingState::Idle)
    {
        UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: Toggle requested, entering fishing mode."));
        EnterFishingStance(Requester); // Call the helper
    }
    else if (CurrentState == EFishingState::ReadyToCast)
    {
        UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: Toggle requested, exiting fishing mode."));
        ResetState();
    }
}

float UFishingSubsystem::GetLineTensionPercent() const
{
    if (MaxLineTension <= 0.f)
    {
        return 0.f;
    }
    return CurrentLineTension / MaxLineTension;
}

void UFishingSubsystem::StartFishingSequence()
{
    float TimeToBite = FMath::RandRange(5.0f, 15.0f);
    GetWorld()->GetTimerManager().SetTimer(FishBiteTimerHandle, this, &UFishingSubsystem::OnFishBite, TimeToBite, false);
}

void UFishingSubsystem::OnFishBite()
{
    if (CurrentState == EFishingState::Fishing)
    {
        CurrentState = EFishingState::FishHooked;
        if (ActiveRod)
        {
            ActiveRod->NotifyFishBite();
        }
        
        // Start the timer. If the player doesn't react in 2 seconds, the fish gets away.
        float HookedTimeLimit = 2.0f; 
        GetWorld()->GetTimerManager().SetTimer(HookedTimerHandle, this, &UFishingSubsystem::OnFishGotAway, HookedTimeLimit, false);
    }
}

void UFishingSubsystem::OnFishGotAway()
{
    // This function only runs if the timer completes.
    if (CurrentState == EFishingState::FishHooked)
    {
        UE_LOG(LogTemp, Log, TEXT("The fish got away..."));
        
        // We go back to fishing, but don't need to reset the whole line.
        CurrentState = EFishingState::Fishing;
        
        // Start waiting for the next bite.
        StartFishingSequence();
    }
}

void UFishingSubsystem::ResetState()
{
    if (CurrentFisher)
    {
        CurrentFisher->SetContinuousAiming(false); // NEW

        if (ASolaraqCharacterPlayerController* PC = Cast<ASolaraqCharacterPlayerController>(CurrentFisher->GetController()))
        {
            PC->HideFishingHUD();
        }
        CurrentFisher->SetContinuousAiming(false);
    }

    if (ActiveRod)
    {
        ActiveRod->NotifyReset();
    }

    CurrentState = EFishingState::Idle;
    CurrentFisher = nullptr;
    ActiveRod = nullptr;
    // ActiveBobber = nullptr; // No longer needed
    
    GetWorld()->GetTimerManager().ClearTimer(FishBiteTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(HookedTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(FishBehaviorTimerHandle);

    bIsFishPulling = false;
    CurrentLineTension = 0.0f;
}

void UFishingSubsystem::OnBobberLandedInWater()
{
    if (CurrentState == EFishingState::Fishing)
    {
        UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: Bobber has landed. Starting fish bite timer."));
        StartFishingSequence();
    }
}
