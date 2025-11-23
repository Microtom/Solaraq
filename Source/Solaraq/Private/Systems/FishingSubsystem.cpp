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
    // 1. Handle Casting Charge
    if (CurrentState == EFishingState::Casting)
    {
        CastCharge = FMath::Clamp(CastCharge + DeltaTime, 0.f, 1.f);
    }
    
    // 2. Handle Reeling Logic
    if (CurrentState == EFishingState::Reeling)
    {
        UpdateTension(DeltaTime);

        // Check if the fish is "caught" (Rope is reeled in fully)
        // With the XPBD fix, ActiveRod->CurrentRopeLength is now accurate.
        if (ActiveRod)
        {
            // If the rope is shorter than ~2 segments, we consider the fish at the boat.
            float CatchThreshold = ActiveRod->RopeSegmentLength * 2.5f;
            
            if (ActiveRod->CurrentRopeLength <= CatchThreshold)
            {
                CatchFish();
            }
        }
    }

    // 3. Movement Check (Cancel fishing if player walks away)
    if (CurrentState != EFishingState::Idle && CurrentFisher)
    {
        // Use a small threshold to avoid cancelling due to tiny animation drifts
        // Bumped to 10.0f to be a bit more forgiving
        if (CurrentFisher->GetVelocity().SizeSquared() > 10.0f)
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

    const bool bIsPlayerReeling = ActiveRod->IsReeling();
    
    // --- NEW LOGIC: Check Physical Slack ---
    // If the rope is physically loose, tension should be low, even if reeling.
    float PhysicalSlack = 0.0f;
    if (ActiveRod->TargetRopeLength > 0)
    {
        // Simple heuristic: If we have particles, compare direct distance vs rope length
        // Note: You might need to expose a "GetStraightLineDistance()" helper in rod
    }

    // Keep existing logic for now, but TUNE THE RATES in the blueprint.
    // The previous logs showed tension going 0->100 in ~10 seconds.
    // Reduce 'TensionIncreaseRate' in the Subsystem Blueprint from 15.0 to something like 5.0.

    if (bIsPlayerReeling)
    {
        CurrentLineTension += TensionIncreaseRate * DeltaTime;
    }
    if (bIsFishPulling)
    {
        CurrentLineTension += FishPullTensionRate * DeltaTime;
    }
    if (!bIsPlayerReeling && !bIsFishPulling)
    {
        CurrentLineTension -= TensionDecreaseRate * DeltaTime;
    }
    
    CurrentLineTension = FMath::Clamp(CurrentLineTension, 0.f, MaxLineTension);

    if (CurrentLineTension >= MaxLineTension)
    {
        OnLineSnap();
        return;
    }
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
        // SAFETY CHECK
            if (ASolaraqCharacterPlayerController* PC = Cast<ASolaraqCharacterPlayerController>(CurrentFisher->GetController()))
            {
                PC->ShowFishingHUD();
            }
		
        // --- FIX: Handle Reeling Empty Line ---
        // Transition to Reeling state so we can pull the bobber back.
        CurrentState = EFishingState::Reeling;
        if (ActiveRod)
        {
            ActiveRod->StartReeling();
        }
		
        // CRITICAL FIX: Add break so we DO NOT fall through to FishHooked.
        // We don't want StartFishBehavior() to run unless a fish is actually on the line.
        break; 
        
    case EFishingState::FishHooked:
        // (Keep existing logic for FishHooked)
            GetWorld()->GetTimerManager().ClearTimer(HookedTimerHandle);
        CurrentState = EFishingState::Reeling;
        GetWorld()->GetTimerManager().ClearTimer(FishBiteTimerHandle);
		
        if (ActiveRod)
        {
            ActiveRod->StartReeling();
        }
        if (ASolaraqCharacterPlayerController* PC = Cast<ASolaraqCharacterPlayerController>(CurrentFisher->GetController()))
        {
            PC->ShowFishingHUD();
        }
		
        // Only start fish behavior here!
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

    // SAFETY CHECK
    if (!FishLootTable)
    {
        UE_LOG(LogSolaraqFishing, Error, TEXT("CatchFish FAILED: FishLootTable is missing in Project Settings (Fishing Subsystem)!"));
        ResetState();
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

    UE_LOG(LogSolaraqFishing, Warning, TEXT("Fish Caught Successfully!"));
    
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
    // --- FISH DISABLED FOR DEBUGGING ---
    UE_LOG(LogSolaraqFishing, Warning, TEXT("DEBUG: Fish Disabled. Waiting for player to reel in manually."));
    //float TimeToBite = FMath::RandRange(5.0f, 15.0f);
    //GetWorld()->GetTimerManager().SetTimer(FishBiteTimerHandle, this, &UFishingSubsystem::OnFishBite, TimeToBite, false);
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
