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
    CollisionComp->SetNotifyRigidBodyCollision(true); // Generate hit events
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
        CollisionComp->OnComponentHit.AddDynamic(this, &ASolaraqProjectile::OnHit);
        UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Projectile %s: OnHit delegate bound."), *GetName());
    }
    else
    {
         UE_LOG(LogSolaraqProjectile, Error, TEXT("Projectile %s: CollisionComp is NULL in BeginPlay! Cannot bind OnHit."), *GetName());
    }

    UE_LOG(LogSolaraqProjectile, Log, TEXT("Projectile %s Spawned. InitialSpeed: %.1f, LifeSpan: %.1f"),
        *GetName(), ProjectileMovement ? ProjectileMovement->InitialSpeed : -1.f, InitialLifeSpan);
}

void ASolaraqProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // Only process hit if it's a valid hit against a different actor/component
    if ((OtherActor != nullptr) && (OtherActor != this) && (OtherComp != nullptr))
    {
         UE_LOG(LogSolaraqProjectile, Log, TEXT("Projectile %s Hit: %s (Component: %s)"),
             *GetName(), *OtherActor->GetName(), *OtherComp->GetName());

        // --- Check if we hit a Solaraq Ship ---
        ASolaraqShipBase* HitShip = Cast<ASolaraqShipBase>(OtherActor);
        if (HitShip)
        {
            UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Projectile %s hit Ship %s!"), *GetName(), *HitShip->GetName());

            // --- Apply Damage (Server Only) ---
            if (HasAuthority()) // Only the server should deal damage
            {
                // Ensure a valid damage type class is set
                TSubclassOf<UDamageType> DmgTypeClass = DamageTypeClass ? DamageTypeClass : TSubclassOf<UDamageType>(UDamageType::StaticClass());

                // Prepare damage event data
                FPointDamageEvent DamageEvent(BaseDamage, Hit, NormalImpulse, DmgTypeClass);

                // Get the controller responsible for this projectile (who fired it)
                AController* InstigatorController = GetInstigatorController();

                UE_LOG(LogSolaraqProjectile, Log, TEXT("Server: Applying %.1f PointDamage to %s from %s (Instigator: %s)"),
                       BaseDamage, *OtherActor->GetName(), *GetNameSafe(this), *GetNameSafe(InstigatorController));

                // Apply the damage
                OtherActor->TakeDamage(BaseDamage, DamageEvent, InstigatorController, this);
            }
        }
        else
        {
            // Optionally handle hitting other things (asteroids, environment, etc.)
            UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Projectile %s hit something other than a ship."), *GetName());
        }

        // --- Destroy the Projectile ---
        // The server is the authority on destruction. Client effects can be triggered via multicast before this if needed.
        if (HasAuthority())
        {
             UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Server: Destroying projectile %s after hit."), *GetName());
            Destroy();
        }
        // If not server, stop the projectile immediately visually and maybe play local FX
        else
        {
            if (ProjectileMovement) ProjectileMovement->StopMovementImmediately();
            SetActorEnableCollision(false); // Stop further hits locally
             // You might play a client-side impact effect here
        }

    } // End safety check for valid hit
}