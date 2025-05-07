// SolaraqProjectile.cpp

#include "Projectiles/SolaraqProjectile.h" // Adjust path as needed

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DamageEvents.h"
#include "Pawns/SolaraqShipBase.h"       // Adjust path as needed
#include "Logging/SolaraqLogChannels.h" // Adjust path as needed
#include "Net/UnrealNetwork.h"          // For HasAuthority()

// Sets default values
ASolaraqProjectile::ASolaraqProjectile()
{
    // --- Basic Actor Setup ---
    PrimaryActorTick.bCanEverTick = false; // Usually projectiles don't need to tick themselves
    bReplicates = true;                   // Replicate this actor
    // SetReplicateMovement(true); // Often handled better by ProjectileMovementComponent's replication

    // --- Create Components ---
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->InitSphereRadius(15.0f); // Set a sensible default radius
    CollisionComp->SetCollisionProfileName(TEXT("Projectile")); // <<< IMPORTANT: Use the custom preset name!
    CollisionComp->SetGenerateOverlapEvents(true); 
    CollisionComp->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.f));
    CollisionComp->CanCharacterStepUpOn = ECB_No;
    CollisionComp->SetIsReplicated(true); // Replicate the collision component state
    // Set as root component
    RootComponent = CollisionComp;

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(CollisionComp); // Attach mesh to collision sphere
    ProjectileMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName); // Mesh is visual only
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComp"));
    ProjectileMovement->UpdatedComponent = CollisionComp; // Component to move
    ProjectileMovement->InitialSpeed = 8000.f;          // Set default initial speed
    ProjectileMovement->MaxSpeed = 8000.f;                // Set default max speed
    ProjectileMovement->bRotationFollowsVelocity = true; // Mesh rotates to face direction
    ProjectileMovement->bShouldBounce = false;            // Don't bounce
    ProjectileMovement->ProjectileGravityScale = 0.f;     // No gravity
    ProjectileMovement->SetIsReplicated(true);            // Replicate movement component

    // --- Set Default Properties ---
    InitialLifeSpan = ProjectileLifeSpan; // Use the UPROPERTY variable
    DamageTypeClass = UDamageType::StaticClass(); // Default damage type
}

// Called when the game starts or when spawned
void ASolaraqProjectile::BeginPlay()
{
    Super::BeginPlay();

    // Bind the OnHit function AFTER components are created and initialized
    if (CollisionComp)
    {
        // CollisionComp->OnComponentHit.AddDynamic(this, &ASolaraqProjectile::OnHit);
        CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ASolaraqProjectile::OnOverlapBegin); // <<< CHANGE THIS
        UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Projectile %s: OnOverlapBegin delegate bound."), *GetName());
    }
    else
    {
        UE_LOG(LogSolaraqProjectile, Error, TEXT("Projectile %s: CollisionComp is NULL in BeginPlay! Cannot bind OnOverlapBegin."), *GetName());
    }

    UE_LOG(LogSolaraqProjectile, Log, TEXT("Projectile %s Spawned. InitialSpeed: %.1f, LifeSpan: %.1f"),
        *GetName(), ProjectileMovement ? ProjectileMovement->InitialSpeed : -1.f, InitialLifeSpan);
}

void ASolaraqProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Only process hit if it's a valid overlap against a different actor/component
    if ((OtherActor != nullptr) && (OtherActor != this) && (OtherComp != nullptr))
    {
         UE_LOG(LogSolaraqProjectile, Log, TEXT("Projectile %s Overlapped: %s (Component: %s)"), // Changed log message
             *GetName(), *OtherActor->GetName(), *OtherComp->GetName());

        ASolaraqShipBase* HitShip = Cast<ASolaraqShipBase>(OtherActor);
        if (HitShip)
        {
            UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Projectile %s overlapped Ship %s!"), *GetName(), *HitShip->GetName());

            if (HasAuthority())
            {
                TSubclassOf<UDamageType> DmgTypeClass = DamageTypeClass ? DamageTypeClass : TSubclassOf<UDamageType>(UDamageType::StaticClass());

                // Use SweepResult for FPointDamageEvent.
                // SweepResult.ImpactNormal can be used for the ShotDirection.
                FPointDamageEvent DamageEvent(BaseDamage, SweepResult, SweepResult.ImpactNormal, DmgTypeClass);

                AController* InstigatorController = GetInstigatorController();
                UE_LOG(LogSolaraqProjectile, Log, TEXT("Server: Applying %.1f PointDamage to %s from %s (Instigator: %s) via Overlap"),
                       BaseDamage, *OtherActor->GetName(), *GetNameSafe(this), *GetNameSafe(InstigatorController));
                OtherActor->TakeDamage(BaseDamage, DamageEvent, InstigatorController, this);
            }
        }
        else
        {
            UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Projectile %s overlapped something other than a ship."), *GetName());
            // If you want projectiles to be destroyed by hitting other things (like asteroids that might also be set to overlap projectiles)
            // you might add destruction logic here too. For now, it only destroys after hitting a ship.
        }

        // Destroy the Projectile on the server.
        // Clients can play effects immediately and then the actor will be destroyed.
        if (HasAuthority())
        {
             UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Server: Destroying projectile %s after overlap."), *GetName());
            Destroy();
        }
        else // Client-side cleanup if needed before server destruction
        {
            if (ProjectileMovement) ProjectileMovement->StopMovementImmediately();
            SetActorEnableCollision(false); // Stop further overlaps locally
            // Play client-side impact effect here if desired
        }
    }
}
