// FishingSubsystem.cpp
#include "Systems/FishingSubsystem.h"

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
    if (CurrentState == EFishingState::Reeling && ActiveRod)
    {
        // A simple condition to check if the fish is "caught"
        if (ActiveRod->CurrentRopeLength <= ActiveRod->RopeSegmentLength * 2.0f)
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

void UFishingSubsystem::RequestPrimaryAction(ASolaraqCharacterPawn* Caster, AItemActor_FishingRod* Rod)
{
    UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: RequestPrimaryAction. Current State: %s"), *UEnum::GetValueAsString(CurrentState));
    switch (CurrentState)
    {
    case EFishingState::ReadyToCast:
    case EFishingState::Idle:
        // This is the "start charging cast" logic. It remains unchanged.
        CurrentState = EFishingState::Casting;
        CurrentFisher = Caster;
        ActiveRod = Rod;
        CastCharge = 0.f;
        break;

        // MODIFIED: This case now handles starting the reel for both a hooked fish AND an empty line.
    case EFishingState::Fishing:
    case EFishingState::FishHooked:
        GetWorld()->GetTimerManager().ClearTimer(HookedTimerHandle); // Stop the "fish got away" timer if it was running.
        GetWorld()->GetTimerManager().ClearTimer(FishBiteTimerHandle); // Stop waiting for a new bite.

        CurrentState = EFishingState::Reeling;
        UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: Primary action in Fishing/Hooked state. Transitioning to Reeling."));

        if (ActiveRod)
        {
            ActiveRod->StartReeling();
        }
        break;
        
    default:
        // Ignore request in other states (like Reeling or already Casting)
            break;
    }
}

void UFishingSubsystem::RequestPrimaryAction_Stop(ASolaraqCharacterPawn* Caster, AItemActor_FishingRod* Rod)
{
    UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: RequestPrimaryAction_Stop. Current State: %s"), *UEnum::GetValueAsString(CurrentState));

    // --- Handle releasing a cast (Unchanged) ---
    if (CurrentState == EFishingState::Casting && Caster == CurrentFisher)
    {
        const FVector AimDirection = Caster->GetAimDirection();

        // This function now spawns the bobber, but we don't start the fishing timer yet.
        Rod->SpawnAndCastBobber(AimDirection, CastCharge);
    
        // We go to a new "waiting for land" state, or just reuse 'Fishing'
        // For simplicity, let's just go to Fishing. The timer won't start until the bobber tells us.
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
        UE_LOG(LogTemp, Warning, TEXT("Subsystem: Requesting to enter fishing mode."));
        
        // --- NEW: USE THE PAWN'S SMOOTH TURN ---
        const FVector AimDirection = Requester->GetAimDirection();
        const FRotator TargetRotation = UKismetMathLibrary::MakeRotFromX(AimDirection);

        UE_LOG(LogTemp, Warning, TEXT("Subsystem: Calculated Aim Direction: %s, resulting in Target Yaw: %.2f"), *AimDirection.ToString(), TargetRotation.Yaw);
        
        UE_LOG(LogTemp, Warning, TEXT("Subsystem: Calling StartSmoothTurn on Pawn."));
        Requester->StartSmoothTurn(TargetRotation);
        // ---

        // Enter fishing mode
        CurrentState = EFishingState::ReadyToCast;
        CurrentFisher = Requester;
        //ActiveRod = Requester->GetEquipmentComponent() ? Cast<AItemActor_FishingRod>(Requester->GetEquipmentComponent()->GetEquippedItemActor()) : nullptr;
        UE_LOG(LogTemp, Warning, TEXT("Subsystem: State changed to ReadyToCast."));
    }
    else if (CurrentState == EFishingState::ReadyToCast)
    {
        UE_LOG(LogTemp, Warning, TEXT("Subsystem: Requesting to exit fishing mode."));
        // The pawn's Tick logic will automatically handle stopping the turn
        // and reverting to bOrientRotationToMovement = true. We don't need to do anything extra.
        
        ResetState();
    }
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
}

void UFishingSubsystem::OnBobberLandedInWater()
{
    if (CurrentState == EFishingState::Fishing)
    {
        UE_LOG(LogSolaraqFishing, Log, TEXT("Subsystem: Bobber has landed. Starting fish bite timer."));
        StartFishingSequence();
    }
}
