#include "Environment/DestructibleBase.h"
#include "Damage/MiningDamageType.h"
#include "Engine/DamageEvents.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h" 
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Logging/SolaraqLogChannels.h" // Assuming this is where LogSolaraqMining lives

ADestructibleBase::ADestructibleBase() :
    MaxHealth(100.0f),
    CurrentHealth(0.0f), 
    MinSignificantDamageToFracture(10.0f), // Restored default
    bIsDestroyed(false),
    ExplosionImpulseStrength(200.0f), 
    DebrisLifeSpan(20.0f),            
    DebrisFadeDuration(5.0f),         
    DestructionParticleSystem(nullptr),
    DestructionSound(nullptr),
    PieceBrokenParticleSystem(nullptr), // Restored init
    PieceBrokenSound(nullptr),          // Restored init
    bIsFadingOut(false),
    TimeSinceFadeStarted(0.0f)
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false; 

    GeometryCollectionComponent = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollection"));
    SetRootComponent(GeometryCollectionComponent);

    GeometryCollectionComponent->SetSimulatePhysics(true);
    GeometryCollectionComponent->SetEnableGravity(false); 
    GeometryCollectionComponent->SetCollisionObjectType(ECC_WorldDynamic);
    GeometryCollectionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GeometryCollectionComponent->SetNotifyBreaks(true); 
}

void ADestructibleBase::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;
    bIsDestroyed = false; 

    if (GeometryCollectionComponent)
    {
        GeometryCollectionComponent->OnChaosBreakEvent.AddDynamic(this, &ADestructibleBase::HandleChaosBreakEvent);
    }
}

void ADestructibleBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Handle Fading Logic
    if (bIsFadingOut && GeometryCollectionComponent)
    {
        TimeSinceFadeStarted += DeltaTime;
        float Alpha = FMath::Clamp(1.0f - (TimeSinceFadeStarted / DebrisFadeDuration), 0.0f, 1.0f);

        // Attempt to set a scalar parameter for Opacity/Dissolve on all materials
        // This requires your material to have a Scalar Parameter named "Dissolve" or "Opacity"
        int32 NumMaterials = GeometryCollectionComponent->GetNumMaterials();
        for (int32 i = 0; i < NumMaterials; ++i)
        {
            // Note: Efficient fading usually creates DMI once, but for destruction cleanup this is acceptable
            UMaterialInstanceDynamic* MatInst = GeometryCollectionComponent->CreateDynamicMaterialInstance(i);
            if (MatInst)
            {
                MatInst->SetScalarParameterValue(FName("Dissolve"), 1.0f - Alpha); // Assuming 0=Solid, 1=Invisible
                MatInst->SetScalarParameterValue(FName("Opacity"), Alpha);         // Assuming 1=Solid, 0=Invisible
            }
        }
    }
}

float ADestructibleBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsDestroyed || DamageAmount <= 0.f) return 0.f;

    const float ActualDamageApplied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    bool bIsMiningDamage = (DamageEvent.DamageTypeClass && DamageEvent.DamageTypeClass->IsChildOf(UMiningDamageType::StaticClass()));

    if (bIsMiningDamage)
    {
        CurrentHealth -= ActualDamageApplied;

        if (CurrentHealth <= 0.f) 
        {
            TriggerFullDestruction(DamageCauser);
        }
    }
    return ActualDamageApplied;
}

void ADestructibleBase::TriggerFullDestruction(AActor* DamageCauser)
{
    if (bIsDestroyed) return;
    PerformFullDestruction(DamageCauser);
}

void ADestructibleBase::PerformFullDestruction(AActor* DamageCauser)
{
    if (bIsDestroyed) return;
    bIsDestroyed = true; 
    CurrentHealth = 0.0f; 

    OnFullyDestroyed(DamageCauser);
    
    // Disable Taking further damage
    SetCanBeDamaged(false);
}

void ADestructibleBase::OnFullyDestroyed_Implementation(AActor* DamageCauser)
{
    // Call the multicast to shatter physics on all clients
    Multicast_Shatter();
}

void ADestructibleBase::Multicast_Shatter_Implementation()
{
    // 1. Play Effects
    if (DestructionParticleSystem)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DestructionParticleSystem, GetActorLocation(), GetActorRotation());
    }
    if (DestructionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation());
    }

    // 2. FORCE PHYSICS SHATTER
    if (GeometryCollectionComponent)
    {
        GeometryCollectionComponent->SetSimulatePhysics(true);
        GeometryCollectionComponent->SetNotifyBreaks(true);
        GeometryCollectionComponent->WakeAllRigidBodies();
        
        // Ensure all pieces are moveable
        GeometryCollectionComponent->SetDynamicState(Chaos::EObjectStateType::Dynamic);

        // Force crumble
        GeometryCollectionComponent->CrumbleActiveClusters();

        // 3. APPLY IMPULSE FROM CENTER
        // Using the Bounding Box Center ensures the explosion pushes pieces OUTWARDS from the middle of the rock
        FBox Bounds = GetComponentsBoundingBox();
        FVector Center = Bounds.GetCenter();
        float Radius = Bounds.GetExtent().GetMax() * 1.5f;

        // Ensure radius isn't too small
        if(Radius < 500.0f) Radius = 500.0f;

        GeometryCollectionComponent->AddRadialImpulse(
            Center, 
            Radius, 
            ExplosionImpulseStrength, // Configurable strength (Default 200)
            ERadialImpulseFalloff::RIF_Linear, 
            true // bVelChange=true ignores mass, providing uniform speed
        );
    }

    // 4. CLEANUP TIMERS
    // Set a hard lifespan so the actor definitely deletes at 20s
    SetLifeSpan(DebrisLifeSpan);

    // Schedule the fade-out start (e.g., at 15s)
    float FadeStartTime = FMath::Max(0.1f, DebrisLifeSpan - DebrisFadeDuration);
    GetWorldTimerManager().SetTimer(TimerHandle_StartFade, this, &ADestructibleBase::StartFadingOut, FadeStartTime, false);

    UE_LOG(LogSolaraqMining, Log, TEXT("DestructibleBase %s: Shattered. Cleanup in %.1fs."), *GetName(), DebrisLifeSpan);
}

void ADestructibleBase::StartFadingOut()
{
    bIsFadingOut = true;
    SetActorTickEnabled(true); // Enable tick to process the fade loop
}

void ADestructibleBase::HandleChaosBreakEvent(const FChaosBreakEvent& BreakEvent)
{
    OnPieceBroken(BreakEvent.Location, BreakEvent.Velocity.GetSafeNormal());
}

void ADestructibleBase::OnPieceBroken_Implementation(const FVector& PieceLocation, const FVector& PieceImpulseDir)
{
    // Play effect for individual piece breaking, but only if we aren't in the middle of the full destruction
    // (to prevent hundreds of particles spawning at once during the main explosion)
    if (!bIsDestroyed)
    {
        if (PieceBrokenParticleSystem)
        {
            UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), PieceBrokenParticleSystem, PieceLocation, PieceImpulseDir.ToOrientationRotator());
        }
        if (PieceBrokenSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, PieceBrokenSound, PieceLocation);
        }
    }
}