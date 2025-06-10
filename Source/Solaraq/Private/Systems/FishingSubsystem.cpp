// FishingSubsystem.cpp
#include "Systems/FishingSubsystem.h"

#include "Items/ItemDataAssetBase.h"
#include "Items/Fishing/FishDataTable.h"
#include "Pawns/SolaraqCharacterPawn.h"
#include "Items/Fishing/ItemActor_FishingRod.h" 
#include "Items/Fishing/FishingBobber.h"  
#include "Kismet/GameplayStatics.h"
#include "Logging/SolaraqLogChannels.h"

void UFishingSubsystem::Tick(float DeltaTime)
{
    if (CurrentState == EFishingState::Casting)
    {
        CastCharge = FMath::Clamp(CastCharge + DeltaTime, 0.f, 1.f);
    }
}

void UFishingSubsystem::RequestPrimaryAction(ASolaraqCharacterPawn* Caster, AItemActor_FishingRod* Rod)
{
    UE_LOG(LogSolaraqFishing, Warning, TEXT("Subsystem: RequestPrimaryAction. Current State: %s"), *UEnum::GetValueAsString(CurrentState));
    switch (CurrentState)
    {
    case EFishingState::Idle:
        // The request is to start casting
            CurrentState = EFishingState::Casting;
        CurrentFisher = Caster;
        ActiveRod = Rod;
        CastCharge = 0.f;
        break;
    case EFishingState::Fishing:
    case EFishingState::FishHooked:
        // The request is to start reeling. THIS IS THE SUCCESS CASE!
        GetWorld()->GetTimerManager().ClearTimer(HookedTimerHandle); // <-- ADD THIS LINE
        
        CurrentState = EFishingState::Reeling;
        GetWorld()->GetTimerManager().ClearTimer(FishBiteTimerHandle); // This is already here, which is fine.
        if (ActiveRod)
        {
            ActiveRod->StartReeling();
        }
        break;
    default:
        // Ignore request in other states (like Reeling or Casting)
            break;
    }
}

void UFishingSubsystem::RequestPrimaryAction_Stop(ASolaraqCharacterPawn* Caster, AItemActor_FishingRod* Rod)
{
    UE_LOG(LogSolaraqFishing, Warning, TEXT("Subsystem: RequestPrimaryAction_Stop. Current State: %s"), *UEnum::GetValueAsString(CurrentState));
    if (CurrentState != EFishingState::Casting || Caster != CurrentFisher)
    {
        UE_LOG(LogSolaraqFishing, Warning, TEXT("Subsystem: ... Ignored due to wrong state or caster."));
        return; // Action is only valid if we are currently casting for this player
    }

    UE_LOG(LogSolaraqFishing, Warning, TEXT("Subsystem: ... Calling SpawnAndCastBobber on Rod (%s)."), *Rod->GetName());
    Rod->SpawnAndCastBobber(CastCharge);
    if (ActiveBobber)
    {
        CurrentState = EFishingState::Fishing;
        UE_LOG(LogSolaraqFishing, Warning, TEXT("Subsystem: ... Bobber is valid. New state: Fishing."));
    }
    else
    {
        UE_LOG(LogSolaraqFishing, Warning, TEXT("Subsystem: ... Bobber is NULL. Resetting state."));
        ResetState();
    }
}

void UFishingSubsystem::OnBobberLanded(AFishingBobber* Bobber, float WaterSurfaceZ)
{
    if (CurrentState == EFishingState::Fishing && Bobber == ActiveBobber)
    {
        Bobber->StartFloating(WaterSurfaceZ);
        StartFishingSequence();
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
        if (ActiveBobber)
        {
            ActiveBobber->Jiggle();
        }
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
    if (ActiveBobber)
    {
        ActiveBobber->Destroy();
    }
    if (ActiveRod)
    {
        ActiveRod->NotifyReset();
    }

    CurrentState = EFishingState::Idle;
    CurrentFisher = nullptr;
    ActiveRod = nullptr;
    ActiveBobber = nullptr;
    GetWorld()->GetTimerManager().ClearTimer(FishBiteTimerHandle);
}
