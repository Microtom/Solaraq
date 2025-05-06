// SolaraqPickupBase.cpp

#include "Gameplay/Pickups/SolaraqPickupBase.h" // Adjust path if needed
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/KismetMathLibrary.h" // For random direction
#include "Pawns/SolaraqShipBase.h" // Include player ship class
#include "Logging/SolaraqLogChannels.h"

ASolaraqPickupBase::ASolaraqPickupBase()
{
    PrimaryActorTick.bCanEverTick = false; // Start false, enable if needed

    // --- Create and Setup Collision Sphere (Root) ---
    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    SetRootComponent(CollisionSphere);
    CollisionSphere->InitSphereRadius(30.0f); // Adjust size as needed
    CollisionSphere->SetMobility(EComponentMobility::Movable);

    // Collision Settings: Overlap only with Pawns, ignore everything else
    CollisionSphere->SetCollisionProfileName(FName("OverlapOnlyPawn"));
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); // Needs physics for impulse, query for overlap
    CollisionSphere->SetGenerateOverlapEvents(true);

    // Physics Settings:
    CollisionSphere->SetSimulatePhysics(true);
    CollisionSphere->SetEnableGravity(false);
    CollisionSphere->SetLinearDamping(LinearDamping); // Set from property
    CollisionSphere->SetAngularDamping(AngularDamping); // Set from property

    // Constraints (keep on 2D plane)
    if (FBodyInstance* BodyInst = CollisionSphere->GetBodyInstance())
    {
        BodyInst->bLockZTranslation = true;
        BodyInst->bLockXRotation = true;
        BodyInst->bLockYRotation = true;
        BodyInst->bLockZRotation = false; // Allow spinning on Z if desired
    }

    // --- Create and Setup Mesh ---
    PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
    PickupMesh->SetupAttachment(RootComponent);
    PickupMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName); // Visual only
    PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PickupMesh->SetGenerateOverlapEvents(false);
    PickupMesh->SetSimulatePhysics(false); // Physics handled by root sphere

    // --- Replication ---
    bReplicates = true;
    // Movement replication is handled by physics replication on the root component
    // SetReplicateMovement(true); // Not needed explicitly when root simulates physics and replicates

    // Bind overlap function
    CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ASolaraqPickupBase::OnOverlapBegin);

    // Set initial lifespan
    InitialLifeSpan = LifeSpanSeconds;

    UE_LOG(LogSolaraqSystem, Verbose, TEXT("ASolaraqPickupBase %s Constructed"), *GetName());
}

void ASolaraqPickupBase::BeginPlay()
{
    Super::BeginPlay();

    // Apply dispersal impulse ONLY on the server after spawning
    if (HasAuthority())
    {
        ApplyDispersalImpulse();
    }
}

void ASolaraqPickupBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    // Add any per-frame logic here if needed (e.g., custom movement stop, visual effects)
}

void ASolaraqPickupBase::ApplyDispersalImpulse()
{
    // --- Server Only Logic ---
    if (!HasAuthority() || !CollisionSphere || !CollisionSphere->IsSimulatingPhysics())
    {
        return;
    }

    // Calculate a random direction on the XY plane
    FVector ImpulseDirection = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(FVector::ForwardVector, 180.0f); // Get random vector in XY plane
    ImpulseDirection.Z = 0; // Ensure Z is zero
    ImpulseDirection.Normalize();

    if (ImpulseDirection.IsNearlyZero()) // Safety check if random vector failed
    {
        ImpulseDirection = FVector::ForwardVector; // Default to forward
    }

    // Apply the impulse
    CollisionSphere->AddImpulse(ImpulseDirection * DispersalImpulseStrength, NAME_None, true); // true = VelocityChange

    UE_LOG(LogSolaraqSystem, Verbose, TEXT("Pickup %s: Applied dispersal impulse %s"), *GetName(), *(ImpulseDirection * DispersalImpulseStrength).ToString());
}

void ASolaraqPickupBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Check if the overlapping actor is a player ship
    ASolaraqShipBase* CollectingShip = Cast<ASolaraqShipBase>(OtherActor);

    // Only proceed if it's a valid ship and it's the ship's main physics body overlapping
    // (Avoid triggering if, e.g., a weapon component overlaps)
    if (CollectingShip && OtherComp == CollectingShip->GetCollisionAndPhysicsRoot())
    {
        // Collection logic MUST run on the server
        if (HasAuthority())
        {
            UE_LOG(LogSolaraqSystem, Log, TEXT("Pickup %s overlapped by Ship %s. Attempting collection..."), *GetName(), *CollectingShip->GetName());
            TryCollect(CollectingShip);
        }
        // else: Client detected overlap, but do nothing. Server handles the actual collection.
    }
}

void ASolaraqPickupBase::TryCollect(ASolaraqShipBase* CollectingShip)
{
    // --- Server Only Logic ---
    if (!HasAuthority() || !CollectingShip)
    {
        return;
    }

    // Call the collection function on the ship
    bool bCollected = CollectingShip->CollectPickup(this->Type, this->Quantity); // Pass type and quantity

    if (bCollected)
    {
        UE_LOG(LogSolaraqSystem, Log, TEXT("Pickup %s successfully collected by Ship %s. Destroying pickup."), *GetName(), *CollectingShip->GetName());
        // Destroy the pickup actor - this destruction replicates to clients
        Destroy();
    }
    else
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("Pickup %s collection FAILED by Ship %s (Ship rejected pickup?)."), *GetName(), *CollectingShip->GetName());
        // Optional: Add a small delay before allowing another collection attempt?
    }
}