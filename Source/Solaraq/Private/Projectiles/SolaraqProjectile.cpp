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

    BaseDamage = 25.0f;
}

void ASolaraqProjectile::SetBaseDamage(float NewDamage)
{
    BaseDamage = NewDamage;
    // UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Projectile %s BaseDamage set to %.1f"), *GetName(), BaseDamage);
}


    UProjectileMovementComponent* ASolaraqProjectile::GetProjectileMovementComponent() const
    {
        return ProjectileMovement;
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

    // Ignore the Instigator (The Ship) so we don't trigger overlaps or physics bumps
    if (CollisionComp && GetInstigator())
    {
        CollisionComp->IgnoreActorWhenMoving(GetInstigator(), true);
        
        // Also tell the Ship to ignore the projectile (Two-way ignore)
        GetInstigator()->MoveIgnoreActorAdd(this);
    }
    
    UE_LOG(LogSolaraqProjectile, Log, TEXT("Projectile %s Spawned. InitialSpeed: %.1f, LifeSpan: %.1f"),
        *GetName(), ProjectileMovement ? ProjectileMovement->InitialSpeed : -1.f, InitialLifeSpan);
}

void ASolaraqProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 1. Basic Validity Checks
    // Ensure we hit a valid actor/component and it is not the bullet itself
    if ((OtherActor == nullptr) || (OtherComp == nullptr) || (OtherActor == this))
    {
        return;
    }

    // 2. SELF-DAMAGE PROTECTION (The Fix)
    // Ignore the Actor that fired this projectile (The Ship)
    if (OtherActor == GetInstigator())
    {
        return;
    }
    // Also ignore the Owner (Ship sets itself as Owner in PerformFireWeapon)
    if (OtherActor == GetOwner())
    {
        return;
    }

    // --- HIT LOGIC STARTS HERE ---

    UE_LOG(LogSolaraqProjectile, Log, TEXT("Projectile %s Overlapped: %s (Component: %s)"), 
         *GetName(), *OtherActor->GetName(), *OtherComp->GetName());

    ASolaraqShipBase* HitShip = Cast<ASolaraqShipBase>(OtherActor);
    
    // Only apply damage/logic on the Server
    if (HasAuthority())
    {
        if (HitShip)
        {
            UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Projectile %s overlapped Ship %s!"), *GetName(), *HitShip->GetName());

            // Determine Damage Type
            TSubclassOf<UDamageType> DmgTypeClass = DamageTypeClass ? DamageTypeClass : TSubclassOf<UDamageType>(UDamageType::StaticClass());

            // Prepare Damage Event
            // Note: If bFromSweep is false, SweepResult might be empty/invalid, 
            // so using the projectile's forward vector for direction is often safer for overlaps.
            FVector ShotDirection = GetActorForwardVector();
            FPointDamageEvent DamageEvent(BaseDamage, SweepResult, ShotDirection, DmgTypeClass);

            AController* InstigatorController = GetInstigatorController();
            
            UE_LOG(LogSolaraqProjectile, Log, TEXT("Server: Applying %.1f PointDamage to %s from %s (Instigator: %s) via Overlap"),
                   BaseDamage, *OtherActor->GetName(), *GetNameSafe(this), *GetNameSafe(InstigatorController));
            
            // Apply the Damage
            OtherActor->TakeDamage(BaseDamage, DamageEvent, InstigatorController, this);
        }
        else
        {
            UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Projectile %s overlapped something other than a ship."), *GetName());
            // Optional: Logic for hitting walls, asteroids, etc.
        }

        // 3. Destroy the Projectile
        // Hide and disable collision immediately so it doesn't hit multiple things while waiting to be destroyed
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
        
        UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Server: Destroying projectile %s after overlap."), *GetName());
        Destroy();
    }
}
