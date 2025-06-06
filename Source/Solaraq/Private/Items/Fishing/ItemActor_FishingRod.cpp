// ItemActor_FishingRod.cpp
#include "Items/Fishing/ItemActor_FishingRod.h"
#include "Items/Fishing/FishingBobber.h"
#include "Pawns/SolaraqCharacterPawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Systems/FishingSubsystem.h"

AItemActor_FishingRod::AItemActor_FishingRod()
{
    // FIXED: Tick MUST be enabled for the line visuals and reeling to work.
    PrimaryActorTick.bCanEverTick = true; 

    // The rest of the constructor is fine...
    if (DefaultSceneRoot)
    {
        DefaultSceneRoot->DestroyComponent();
        DefaultSceneRoot = nullptr;
    }
    RodMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RodMesh"));
    RootComponent = RodMesh;
    FishingLineSpline = CreateDefaultSubobject<USplineComponent>(TEXT("FishingLineSpline"));
    FishingLineSpline->SetupAttachment(RootComponent);
    FishingLineMesh = CreateDefaultSubobject<USplineMeshComponent>(TEXT("FishingLineMesh"));
    FishingLineMesh->SetupAttachment(FishingLineSpline);
    FishingLineMesh->SetMobility(EComponentMobility::Movable);
    FishingLineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AItemActor_FishingRod::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bIsReeling && CurrentBobber)
    {
        FVector RodTipLocation = RodMesh->GetSocketLocation(RodTipSocketName);
        FVector DirectionToRod = (RodTipLocation - CurrentBobber->GetActorLocation()).GetSafeNormal();
        CurrentBobber->ProjectileMovement->Velocity = DirectionToRod * ReelSpeed;

        if (FVector::Dist(RodTipLocation, CurrentBobber->GetActorLocation()) < 100.f)
        {
            // We've finished reeling. The subsystem will handle the state reset.
            if(CurrentBobber) CurrentBobber->Destroy();
            CurrentBobber = nullptr;
            bIsReeling = false;
        }
    }

    UpdateLine(); // Update the visual line every frame
}

void AItemActor_FishingRod::OnUnequip()
{
    // CRITICAL: Tell the subsystem we are being unequipped!
    if (UFishingSubsystem* FishingSS = GetWorld()->GetSubsystem<UFishingSubsystem>())
    {
        FishingSS->OnToolUnequipped(this);
    }

    Super::OnUnequip();
}

void AItemActor_FishingRod::PrimaryUse()
{
    if (bIsReeling) return; // Local check to prevent spamming while reeling animation is playing.

    if (UFishingSubsystem* FishingSS = GetWorld()->GetSubsystem<UFishingSubsystem>())
    {
        FishingSS->RequestPrimaryAction(OwningPawn, this);
    }
}

void AItemActor_FishingRod::PrimaryUse_Stop()
{
    if (UFishingSubsystem* FishingSS = GetWorld()->GetSubsystem<UFishingSubsystem>())
    {
        // Tell the subsystem the player released the cast button.
        FishingSS->RequestPrimaryAction_Stop(OwningPawn, this);
    }
}
AFishingBobber* AItemActor_FishingRod::SpawnAndCastBobber(float Charge)
{
    if (!BobberClass || !OwningPawn) return nullptr;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = OwningPawn;
    
    FVector RodTipLocation = RodMesh->GetSocketLocation(RodTipSocketName);
    CurrentBobber = GetWorld()->SpawnActor<AFishingBobber>(BobberClass, RodTipLocation, OwningPawn->GetControlRotation(), SpawnParams);

    if (CurrentBobber)
    {
        FVector LaunchDir = OwningPawn->GetControlRotation().Vector();
        CurrentBobber->ProjectileMovement->Velocity = LaunchDir * CastPower * FMath::Max(0.2f, Charge);
    }
    return CurrentBobber;
}

void AItemActor_FishingRod::StartReeling()
{
    bIsReeling = true;
}

void AItemActor_FishingRod::NotifyFishBite()
{
    if (FishBiteSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, FishBiteSound, GetActorLocation());
    }
}

void AItemActor_FishingRod::NotifyReset()
{
    if(CurrentBobber) CurrentBobber->Destroy();
    CurrentBobber = nullptr;
    bIsReeling = false;
}
void AItemActor_FishingRod::UpdateLine()
{
    if (!CurrentBobber || !FishingLineStaticMesh)
    {
        FishingLineMesh->SetVisibility(false);
        return;
    }

    FishingLineMesh->SetVisibility(true);
    FishingLineMesh->SetStaticMesh(FishingLineStaticMesh);
    FishingLineMesh->SetMaterial(0, FishingLineMaterial);

    const FVector RodTipLocation = RodMesh->GetSocketLocation(RodTipSocketName);
    const FVector BobberLocation = CurrentBobber->GetActorLocation();

    // Update the spline points
    FishingLineSpline->SetLocationAtSplinePoint(0, RodTipLocation, ESplineCoordinateSpace::World);
    FishingLineSpline->SetLocationAtSplinePoint(1, BobberLocation, ESplineCoordinateSpace::World);

    // This is the trick for the "sag" in the line
    FVector MidPoint = (RodTipLocation + BobberLocation) / 2;
    float Sag = FVector::Dist(RodTipLocation, BobberLocation) * 0.1f; // Adjust 0.1 for more/less sag
    MidPoint.Z -= Sag;

    // We need to tell the spline how to curve. We do this with tangents.
    // Set tangents to make it sag downwards
    FishingLineSpline->SetTangentAtSplinePoint(0, FVector(0,0,-1) * Sag * 2.f, ESplineCoordinateSpace::World);
    FishingLineSpline->SetTangentAtSplinePoint(1, FVector(0,0,-1) * Sag * 2.f, ESplineCoordinateSpace::World);


    // Update the spline mesh component to draw along this new path
    FVector StartPos, StartTangent, EndPos, EndTangent;
    FishingLineSpline->GetLocationAndTangentAtSplinePoint(0, StartPos, StartTangent, ESplineCoordinateSpace::Local);
    FishingLineSpline->GetLocationAndTangentAtSplinePoint(1, EndPos, EndTangent, ESplineCoordinateSpace::Local);
    
    FishingLineMesh->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true);
}


