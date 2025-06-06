// FishingSubsystem.cpp
#include "Systems/FishingSubsystem.h"
#include "Pawns/SolaraqCharacterPawn.h"
#include "Items/Fishing/ItemActor_FishingRod.h" 
#include "Items/Fishing/FishingBobber.h"  
#include "Kismet/GameplayStatics.h"

void UFishingSubsystem::Tick(float DeltaTime)
{
    if (CurrentState == EFishingState::Casting)
    {
        CastCharge = FMath::Clamp(CastCharge + DeltaTime, 0.f, 1.f);
    }
}

void UFishingSubsystem::RequestPrimaryAction(ASolaraqCharacterPawn* Caster, AItemActor_FishingRod* Rod)
{
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
        // The request is to start reeling
        CurrentState = EFishingState::Reeling;
        GetWorld()->GetTimerManager().ClearTimer(FishBiteTimerHandle);
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
    if (CurrentState != EFishingState::Casting || Caster != CurrentFisher)
    {
        return; // Action is only valid if we are currently casting for this player
    }
    
    ActiveBobber = Rod->SpawnAndCastBobber(CastCharge);
    if (ActiveBobber)
    {
        CurrentState = EFishingState::Fishing;
    }
    else
    {
        ResetState();
    }
}

void UFishingSubsystem::OnBobberLanded(AFishingBobber* Bobber, float WaterSurfaceZ)
{
    if (CurrentState == EFishingState::Fishing && Bobber == ActiveBobber)
    {
        Bobber->StartFloating(WaterSurfaceZ);
        StartFishCheck();
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

void UFishingSubsystem::StartFishCheck()
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