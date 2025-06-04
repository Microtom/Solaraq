#include "Environment/DestructibleBase.h"

#include "Damage/MiningDamageType.h"
#include "Engine/DamageEvents.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h" // For FChaosBreakEvent (though forward declare is often enough for header)
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsSettings.h"
// #include "Logging/SolaraqLogChannels.h" // If you have custom log channels

ADestructibleBase::ADestructibleBase() :
    MaxHealth(100.0f),
    CurrentHealth(0.0f), // Will be set in BeginPlay
    MinSignificantDamageToFracture(25.0f),
    bIsDestroyed(false),
    DestructionParticleSystem(nullptr),
    DestructionSound(nullptr),
    PieceBrokenParticleSystem(nullptr),
    PieceBrokenSound(nullptr)
{
    PrimaryActorTick.bCanEverTick = false; // Default to false; enable if needed by derived classes or specific logic

    GeometryCollectionComponent = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollection"));
    SetRootComponent(GeometryCollectionComponent);

    // Common default settings for the Geometry Collection
    GeometryCollectionComponent->SetSimulatePhysics(true);
    GeometryCollectionComponent->SetEnableGravity(false); // Common for space objects
    GeometryCollectionComponent->SetCollisionObjectType(ECC_WorldDynamic); // Consider a custom ECC_Destructible
    GeometryCollectionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GeometryCollectionComponent->SetNotifyBreaks(true); // Essential for HandleChaosBreakEvent
    // GeometryCollectionComponent->SetGenerateOverlapEvents(true); // If you need overlaps for other reasons
}

void ADestructibleBase::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;
    bIsDestroyed = false; // Ensure clean state on BeginPlay

    if (GeometryCollectionComponent)
    {
        // Bind to the chaos break event
        GeometryCollectionComponent->OnChaosBreakEvent.AddDynamic(this, &ADestructibleBase::HandleChaosBreakEvent);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("DestructibleBase %s is missing its GeometryCollectionComponent! Destruction will not work."), *GetName());
    }
}

float ADestructibleBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsDestroyed || DamageAmount <= 0.f)
    {
        return 0.f; // No damage if already destroyed or damage is non-positive
    }

    const float ActualDamageApplied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    if (DamageEvent.DamageTypeClass && DamageEvent.DamageTypeClass->IsChildOf(UMiningDamageType::StaticClass()))
    {
        CurrentHealth -= ActualDamageApplied;
        UE_LOG(LogTemp, Log, TEXT("Destructible '%s' (Mining Damage): Took %.2f damage. Health: %.2f/%.2f"),
               *GetName(), ActualDamageApplied, CurrentHealth, MaxHealth);

        if (CurrentHealth <= 0.f) // Only trigger full destruction if health is depleted by mining
        {
            TriggerFullDestruction(DamageCauser);
        }
        // If ActualDamageApplied >= MinSignificantDamageToFracture and it's mining damage,
        // you could also consider this a trigger for destruction, or just let health deplete.
        // For now, let's keep it simple: mining damage depletes health, zero health = destruction.
        // else if (ActualDamageApplied >= MinSignificantDamageToFracture)
        // {
        //     TriggerFullDestruction(DamageCauser);
        // }

    }
    else
    {
        // Damage is not from a mining source (e.g., ship collision, weapon fire)
        UE_LOG(LogTemp, Log, TEXT("Destructible '%s' (Non-Mining Damage): Took %.2f damage. No health reduction or structural damage applied."),
               *GetName(), ActualDamageApplied);
        return 0.f;
    }
    

    return ActualDamageApplied;
}

void ADestructibleBase::TriggerFullDestruction(AActor* DamageCauser)
{
    if (bIsDestroyed)
    {
        return; // Already destroyed
    }
    PerformFullDestruction(DamageCauser);
}


void ADestructibleBase::PerformFullDestruction(AActor* DamageCauser)
{
    if (bIsDestroyed) // Double-check to prevent re-entry
    {
        return;
    }
    bIsDestroyed = true; // Set state immediately
    CurrentHealth = 0.0f; // Ensure health is zeroed

    // UE_LOG(LogSolaraq, Log, TEXT("Destructible '%s' is being fully destroyed."), *GetName());
    UE_LOG(LogTemp, Log, TEXT("Destructible '%s' is being fully destroyed."), *GetName());


    // Call the BlueprintNativeEvent, allowing Blueprint or C++ overrides to add custom logic
    OnFullyDestroyed(DamageCauser);

    // Disable further damage
    SetCanBeDamaged(false);

    // The actual shattering is primarily handled by the Geometry Collection's response to damage.
    // If damage alone isn't enough (e.g., if it's just "marked" for destruction),
    // you might need to apply a strong impulse or use a field system command here
    // to ensure the GC shatters appropriately. However, ApplyRadialDamage from projectiles
    // usually does a good job if the GC is configured to receive it.

    // Optional: Set a lifespan for the actor itself if debris is handled separately or globally.
    // If you want the root actor to linger while pieces fly, manage its destruction carefully.
    // Often, Chaos debris lifetime CVars are preferred for managing the pieces.
    // SetLifeSpan(15.0f); // Example: actor cleans itself up after 15 seconds
}

void ADestructibleBase::OnFullyDestroyed_Implementation(AActor* DamageCauser)
{
    // Default C++ implementation for OnFullyDestroyed
    // UE_LOG(LogSolaraq, Log, TEXT("DestructibleBase %s: OnFullyDestroyed_Implementation. Playing effects."), *GetName());
    UE_LOG(LogTemp, Log, TEXT("DestructibleBase %s: OnFullyDestroyed_Implementation. Playing effects."), *GetName());


    // Play global destruction effects
    if (DestructionParticleSystem)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DestructionParticleSystem, GetActorLocation(), GetActorRotation());
    }
    if (DestructionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation());
    }

    // The Geometry Collection should be breaking apart due to the damage that led to this state.
    // If not, additional force application might be needed here or in the damage application step.
}

void ADestructibleBase::HandleChaosBreakEvent(const FChaosBreakEvent& BreakEvent)
{
    // This event fires for each cluster that breaks off from the main Geometry Collection.
    // UE_LOG(LogSolaraq, Verbose, TEXT("Destructible %s: Chaos Break Event at %s, Mass: %.2f"),
    //        *GetName(), *BreakEvent.Location.ToString(), BreakEvent.Mass);
    UE_LOG(LogTemp, Verbose, TEXT("Destructible %s: Chaos Break Event at %s, Mass: %.2f"),
           *GetName(), *BreakEvent.Location.ToString(), BreakEvent.Mass);


    // Call the BlueprintNativeEvent for piece breaking
    OnPieceBroken(BreakEvent.Location, BreakEvent.Velocity.GetSafeNormal()); // Velocity can give an impulse direction
}

void ADestructibleBase::OnPieceBroken_Implementation(const FVector& PieceLocation, const FVector& PieceImpulseDir)
{
    // Default C++ implementation for OnPieceBroken
    // UE_LOG(LogSolaraq, Verbose, TEXT("DestructibleBase %s: OnPieceBroken_Implementation at %s"), *GetName(), *PieceLocation.ToString());
    UE_LOG(LogTemp, Verbose, TEXT("DestructibleBase %s: OnPieceBroken_Implementation at %s"), *GetName(), *PieceLocation.ToString());


    if (PieceBrokenParticleSystem)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), PieceBrokenParticleSystem, PieceLocation, PieceImpulseDir.ToOrientationRotator());
    }
    if (PieceBrokenSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, PieceBrokenSound, PieceLocation);
    }
}