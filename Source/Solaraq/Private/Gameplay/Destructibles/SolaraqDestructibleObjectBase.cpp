// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/Destructibles/SolaraqDestructibleObjectBase.h" // Adjust path
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Components/SolaraqGimbalGunComponent.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundCue.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DamageEvents.h"
#include "Field/FieldSystemComponent.h"
#include "Field/FieldSystemObjects.h"
#include "Logging/SolaraqLogChannels.h"
// #include "Field/FieldSystemObjects.h" // Include if using advanced field systems directly from C++

// Logging Helper Macro (ensure you have this defined, e.g., in SolaraqLogChannels.h or a PCH)
#ifndef NET_LOG_DEST
#define NET_LOG_DEST(LogCat, Verbosity, Format, ...) \
UE_LOG(LogCat, Verbosity, TEXT("[%s] %s (Destructible): " Format), \
(GetNetMode() == NM_Client ? TEXT("CLIENT") : (GetNetMode() == NM_ListenServer || GetNetMode() == NM_DedicatedServer ? TEXT("SERVER") : TEXT("STANDALONE"))), \
*FString(__FUNCTION__), \
##__VA_ARGS__)
#endif

ASolaraqDestructibleObjectBase::ASolaraqDestructibleObjectBase()
{
    PrimaryActorTick.bCanEverTick = false;

    GeometryCollectionComponent = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollection"));
    RootComponent = GeometryCollectionComponent;
    // Ensure the Geometry Collection itself can be damaged or affected by physics to break
    // These settings are often configured on the Geometry Collection asset itself or the component in BP.
    GeometryCollectionComponent->SetNotifyBreaks(true); // Essential for OnChaosPhysicsBreak
    GeometryCollectionComponent->bNotifyCollisions = true;  // For OnChaosPhysicsCollision (which triggers OnComponentHit if bound, or BP events)

    // Set a default collision profile. This is important.
    // "PhysicsActor" is a common one for dynamic objects.
    // Or create a custom "Destructible" profile in Project Settings -> Collision.
    GeometryCollectionComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName); // Or a custom "Destructible" profile

    GimbalGunComponent = CreateDefaultSubobject<USolaraqGimbalGunComponent>(TEXT("GimbalGun"));
    GimbalGunComponent->SetupAttachment(RootComponent); // Attach to the GC root
    GimbalGunComponent->SetIsReplicated(true);

    bReplicates = true;
    // For Geometry Collections, replication of the pieces is handled by Chaos,
    // but the actor itself might be static or have minimal movement replication.
    SetReplicateMovement(false);

    CurrentHealth_Internal = MaxHealth;
    bIsDestroyed_Internal = false;
}

void ASolaraqDestructibleObjectBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASolaraqDestructibleObjectBase, CurrentHealth_Internal);
    DOREPLIFETIME(ASolaraqDestructibleObjectBase, bIsDestroyed_Internal);
}

void ASolaraqDestructibleObjectBase::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        CurrentHealth_Internal = MaxHealth;
        bIsDestroyed_Internal = false;
    }

    if (GeometryCollectionComponent)
    {
        // Bind to Chaos events
        GeometryCollectionComponent->OnChaosBreakEvent.AddDynamic(this, &ASolaraqDestructibleObjectBase::OnChaosPhysicsBreak);
        // If you want to react to physics collisions against the GC *before* it breaks (or between its pieces and other things)
        GeometryCollectionComponent->OnComponentHit.AddDynamic(this, &ASolaraqDestructibleObjectBase::OnGeometryCollectionHit);
    }

    if (GimbalGunComponent)
    {
        GimbalGunComponent->SetVisibility(bCanHostGimbalGun, true);
        GimbalGunComponent->SetComponentTickEnabled(bCanHostGimbalGun);
        if (!bCanHostGimbalGun)
        {
            GimbalGunComponent->Deactivate();
        }
        else
        {
            // OwningPawn setup for the gun needs careful consideration
            // Since this actor isn't a Pawn, the gun's current SetOwningPawn might not work directly.
            // The gun should be modified to get TeamID from its AActor owner if it implements IGenericTeamAgentInterface.
            // For now, the gun might not have a valid OwningPawn if attached to this.
            // It can still get its team from this actor via GetOwner()->GetGenericTeamId() if the gun is adapted.
        }
    }
}

float ASolaraqDestructibleObjectBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsDestroyed_Internal)
    {
        return 0.0f;
    }

    const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    if (ActualDamage > 0.f)
    {
        NET_LOG_DEST(LogSolaraqCombat, Log, TEXT("Actor %s taking %.1f damage from %s. CurrentHealth: %.1f / %.1f"),
            *GetName(), ActualDamage, *GetNameSafe(DamageCauser), CurrentHealth_Internal - ActualDamage, MaxHealth); // Log before subtraction for clarity

        if (HasAuthority())
        {
            CurrentHealth_Internal -= ActualDamage;
            CurrentHealth_Internal = FMath::Max(0.0f, CurrentHealth_Internal);

            if (CurrentHealth_Internal <= 0.0f)
            {
                HandleDestruction(DamageCauser, DamageEvent);
            }
            // OnRep_CurrentHealth will be called on clients due to replication.
        }
    }
    return ActualDamage;
}

void ASolaraqDestructibleObjectBase::HandleDestruction(AActor* DamageCauser, const FDamageEvent& InstigatingDamageEvent)
{
    if (!HasAuthority() || bIsDestroyed_Internal)
    {
        return;
    }

    NET_LOG_DEST(LogSolaraqCombat, Log, TEXT("Actor %s destroyed by %s! Triggering Chaos Destruction."), *GetName(), *GetNameSafe(DamageCauser));
    bIsDestroyed_Internal = true;

    Multicast_PlayMainDestructionEffects();

    if (GetWorld()) 
    {
        FVector ForceOrigin = GetActorLocation();
        NET_LOG_DEST(LogSolaraqCombat, Verbose, TEXT("Chaos radial force origin set to ActorLocation: %s"), *ForceOrigin.ToString());

        AFieldSystemActor* FieldActor = GetWorld()->SpawnActor<AFieldSystemActor>(ForceOrigin, FRotator::ZeroRotator);
        if (FieldActor)
        {
            UFieldSystemComponent* FieldSystemComponent = FieldActor->GetFieldSystemComponent();
            if (FieldSystemComponent)
            {
                // Using the ApplyRadialForce function you found.
                // Note: This function does not take a radius or falloff directly.
                // The force will emanate from 'Position' with 'Magnitude'.
                // The effective radius/falloff might be implicitly handled by Chaos or be non-existent for this simple call.
                // We control the "impulse" nature by making the FieldActor very short-lived.
                FieldSystemComponent->ApplyRadialForce(
                    true,                             // bEnabled
                    ForceOrigin,                      // Position of the force center
                    ChaosDestructionImpulseStrength   // Magnitude of the force (treat as impulse strength due to short duration)
                );

                NET_LOG_DEST(LogSolaraqSystem, Verbose, TEXT("Called ApplyRadialForce on FieldSystemComponent. Magnitude: %.1f at %s"),
                    ChaosDestructionImpulseStrength, *ForceOrigin.ToString());
                
                // Make the field actor very short-lived to approximate an impulse.
                // The force is applied as long as the FieldSystemComponent is active and enabled.
                FieldActor->SetLifeSpan(0.1f); // Very short, e.g., a few physics ticks
            }
            else
            {
                NET_LOG_DEST(LogSolaraqSystem, Error, TEXT("Failed to get FieldSystemComponent from spawned FieldActor for %s"), *GetName());
                if(FieldActor) FieldActor->Destroy(); 
            }
        }
        else
        {
            NET_LOG_DEST(LogSolaraqSystem, Error, TEXT("Failed to spawn FieldSystemActor for %s"), *GetName());
        }
    }

    if (GimbalGunComponent && GimbalGunComponent->IsActive())
    {
        GimbalGunComponent->Deactivate();
        GimbalGunComponent->SetVisibility(false);
    }

    SetLifeSpan(TimeToDestroyActorAfterChaos);
}

void ASolaraqDestructibleObjectBase::Multicast_PlayMainDestructionEffects_Implementation()
{
    NET_LOG_DEST(LogSolaraqSystem, Log, TEXT("Playing MAIN destruction effects for %s (e.g. large explosion)"), *GetName());
    if (DestructionEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DestructionEffect, GetActorLocation(), GetActorRotation());
    }
    if (DestructionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation());
    }
    // Do NOT hide the GeometryCollectionComponent here. Chaos manages its pieces.
    // If the GimbalGunComponent is separate and should vanish with the main explosion, hide it:
    if(GimbalGunComponent && GimbalGunComponent->IsVisible() && bIsDestroyed_Internal) // Check bIsDestroyed to ensure it's part of destruction sequence
    {
        GimbalGunComponent->SetVisibility(false, true);
    }
}

void ASolaraqDestructibleObjectBase::OnChaosPhysicsBreak(const FChaosBreakEvent& BreakEvent)
{
    // This event fires on the server and all clients if the GeometryCollectionComponent is set to replicate breaks.
    NET_LOG_DEST(LogSolaraqSystem, Verbose, TEXT("Chaos chunk break event for %s at %s. Component: %s, Mass: %.2f"),
        *GetNameSafe(this), 
        *BreakEvent.Location.ToString(), // Use BreakEvent.Location
        *GetNameSafe(BreakEvent.Component.Get()), // Use BreakEvent.Component
        BreakEvent.Mass); // Example of accessing another member

    if (ChunkBreakEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ChunkBreakEffect, BreakEvent.Location, FRotator::ZeroRotator, true); // Use BreakEvent.Location
    }
    if (ChunkBreakSound)
    {
        // Optional: Play sound only if the broken piece has significant mass, to avoid too many tiny sounds
        if (BreakEvent.Mass > 0.1f) // Adjust mass threshold as needed
        {
            UGameplayStatics::PlaySoundAtLocation(GetWorld(), ChunkBreakSound, BreakEvent.Location); // Use BreakEvent.Location
        }
    }
}

void ASolaraqDestructibleObjectBase::OnGeometryCollectionHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    // This function is called when the GeometryCollectionComponent (or its pieces) are hit by other physics objects.
    // You might use this to apply small amounts of damage from physical impacts if the object isn't fully destroyed yet,
    // or to trigger additional effects if pieces collide violently.

    if (bIsDestroyed_Internal) // If already destroyed, pieces are just flying around
    {
        // NET_LOG_DEST(LogSolaraqSystem, VeryVerbose, TEXT("Destroyed GC %s piece hit by %s."), *GetName(), *GetNameSafe(OtherActor));
        return;
    }

    // Example: Apply damage from physical impacts before full destruction
    if (HasAuthority() && OtherActor && OtherActor != this)
    {
        // A very basic damage calculation from impact force.
        // Tune the multiplier carefully.
        float ImpactDamageMultiplier = 0.0001f; // Needs tuning
        float DamageFromImpact = NormalImpulse.Size() * ImpactDamageMultiplier;

        // Apply a minimum threshold for impact damage
        float MinImpactDamageToApply = 1.0f;

        if (DamageFromImpact >= MinImpactDamageToApply)
        {
            NET_LOG_DEST(LogSolaraqCombat, Log, TEXT("%s (GC) hit by %s. Impulse: %s. Applying %.1f impact damage."),
                *GetName(), *GetNameSafe(OtherActor), *NormalImpulse.ToString(), DamageFromImpact);

            FPointDamageEvent DamageEvent(DamageFromImpact, Hit, Hit.ImpactNormal, nullptr);
            AController* InstigatorController = nullptr;
            if (APawn* OtherPawn = Cast<APawn>(OtherActor))
            {
                InstigatorController = OtherPawn->GetController();
            }
            TakeDamage(DamageFromImpact, DamageEvent, InstigatorController, OtherActor);
        }
    }
}


void ASolaraqDestructibleObjectBase::OnRep_CurrentHealth()
{
    // Update UI or client-side visual cues based on health
    // For example, a health bar or progressive damage decals.
    // NET_LOG_DEST(LogSolaraqSystem, Verbose, TEXT("CLIENT %s: OnRep_CurrentHealth: %.1f"), *GetName(), CurrentHealth_Internal);
}

void ASolaraqDestructibleObjectBase::OnRep_IsDestroyed()
{
    NET_LOG_DEST(LogSolaraqSystem, Log, TEXT("CLIENT %s: OnRep_IsDestroyed. State: %d"), *GetName(), bIsDestroyed_Internal);
    if (bIsDestroyed_Internal)
    {
        // Client-side reaction to destruction.
        // The main visual shattering is handled by Chaos replication.
        // This OnRep is more for cleaning up other actor state or triggering UI.

        // If the gun component is still visible for some reason, hide it.
        if (GimbalGunComponent && GimbalGunComponent->IsVisible())
        {
            GimbalGunComponent->SetVisibility(false, true);
            GimbalGunComponent->Deactivate(); // Ensure it's not ticking
        }
        
        // If the GC itself needs explicit hiding on clients (though Chaos usually handles this by pieces flying away)
        // if (GeometryCollectionComponent && GeometryCollectionComponent->IsVisible())
        // {
        //    GeometryCollectionComponent->SetVisibility(false);
        // }
    }
}

float ASolaraqDestructibleObjectBase::GetHealthPercentage() const
{
    if (MaxHealth <= 0.0f) return 0.0f;
    return FMath::Max(0.0f, CurrentHealth_Internal / MaxHealth);
}