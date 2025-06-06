// FishingBobber.cpp
#include "Items/Fishing/FishingBobber.h"
#include "Components/SphereComponent.h"             // FIXED: Added include
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h" // FIXED: Added include
#include "Systems/FishingSubsystem.h" 

AFishingBobber::AFishingBobber()
{
    PrimaryActorTick.bCanEverTick = true; // For buoyancy

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
    RootComponent = CollisionComponent;
    CollisionComponent->SetSphereRadius(8.f);
    CollisionComponent->SetNotifyRigidBodyCollision(true); // REQUIRED for OnComponentHit to fire

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

    if (bIsInWater)
    {
        FVector CurrentLocation = GetActorLocation();
        if (CurrentLocation.Z < WaterLevel)
        {
            float Depth = WaterLevel - CurrentLocation.Z;
            FVector BuoyancyForce = FVector(0.f, 0.f, Depth * 150.f); 
            ProjectileMovement->AddForce(BuoyancyForce);
        }
    }
}

// FIXED: The signature now matches the header and the delegate
void AFishingBobber::OnBobberHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // Check if we hit a reasonably horizontal surface, which we'll assume is water.
    // Ensure we don't trigger this on vertical walls.
    if (Hit.ImpactNormal.Z > 0.7) // A value of 1.0 is perfectly flat. 0.7 allows for some slope.
    {
        if (UFishingSubsystem* FishingSS = GetWorld()->GetSubsystem<UFishingSubsystem>())
        {
            FishingSS->OnBobberLanded(this, Hit.ImpactPoint.Z);
            
            // To prevent this from firing multiple times, we can unbind it after the first valid hit.
            CollisionComponent->OnComponentHit.RemoveDynamic(this, &AFishingBobber::OnBobberHit);
        }
    }
}

void AFishingBobber::StartFloating(float WaterSurfaceZ)
{
    bIsInWater = true;
    WaterLevel = WaterSurfaceZ;
    ProjectileMovement->Velocity *= 0.1f;
}

void AFishingBobber::Jiggle()
{
    ProjectileMovement->AddForce(FVector(0,0, -200.f));
}