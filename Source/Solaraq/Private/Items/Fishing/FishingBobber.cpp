// FishingBobber.cpp
#include "Items/Fishing/FishingBobber.h"
#include "Components/SphereComponent.h"             // FIXED: Added include
#include "Components/StaticMeshComponent.h"
#include "Items/Fishing/ItemActor_FishingRod.h"
#include "GameFramework/ProjectileMovementComponent.h" // FIXED: Added include
#include "Logging/SolaraqLogChannels.h"
#include "Systems/FishingSubsystem.h" 

AFishingBobber::AFishingBobber()
{
    UE_LOG(LogSolaraqFishing, Warning, TEXT("Bobber: Constructor called for a new instance."));
    PrimaryActorTick.bCanEverTick = true; // For buoyancy

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
    RootComponent = CollisionComponent;
    CollisionComponent->SetSphereRadius(8.f);
    CollisionComponent->SetNotifyRigidBodyCollision(true); // REQUIRED for OnComponentHit to fire

#define ECC_FishingLine ECC_GameTraceChannel1
    
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionComponent->SetCollisionObjectType(ECC_Pawn); // Or another appropriate type for a dynamic object
    CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    CollisionComponent->SetCollisionResponseToChannel(ECC_FishingLine, ECR_Ignore);
    
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(RootComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->bShouldBounce = true;
    ProjectileMovement->Bounciness = 0.2f;

    // FIXED: Bind to the correct delegate for world collision
    CollisionComponent->OnComponentHit.AddDynamic(this, &AFishingBobber::OnBobberHit);
}

// FIXED: Added back the Tick function for buoyancy logic
void AFishingBobber::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

}

// FIXED: The signature now matches the header and the delegate
void AFishingBobber::OnBobberHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (Hit.ImpactNormal.Z > 0.7)
    {
        if (AItemActor_FishingRod* OwningRod = Cast<AItemActor_FishingRod>(GetOwner()))
        {
            OwningRod->NotifyBobberLanded();
        }
        if (UFishingSubsystem* FishingSS = GetWorld()->GetSubsystem<UFishingSubsystem>())
        {
            FishingSS->OnBobberLandedInWater();
        }
        
        // --- KEY CHANGE: NUKE THE BOBBER'S PHYSICS ---
        ProjectileMovement->StopMovementImmediately();
        ProjectileMovement->SetComponentTickEnabled(false);
        // Make the collision sphere a non-simulating "ghost" so it doesn't fight the rope.
        CollisionComponent->SetSimulatePhysics(false);
        
        // We no longer need our custom buoyancy logic, as the rope handles everything.
        // StartFloating(Hit.ImpactPoint.Z); // REMOVE THIS
        bIsInWater = false; // Disable the Tick logic for buoyancy.

        CollisionComponent->OnComponentHit.RemoveDynamic(this, &AFishingBobber::OnBobberHit);
    }
}

void AFishingBobber::StartFloating(float WaterSurfaceZ)
{
    UE_LOG(LogSolaraqFishing, Warning, TEXT("Bobber (%s): StartFloating called."), *GetName());
    bIsInWater = true;
    WaterLevel = WaterSurfaceZ;
    ProjectileMovement->Velocity *= 0.1f;
}

void AFishingBobber::Jiggle()
{
    ProjectileMovement->AddForce(FVector(0,0, -200.f));
}