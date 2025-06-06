// SolaraqShipBase.cpp

#include "Pawns/SolaraqShipBase.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SphereComponent.h" // Changed from BoxComponent to SphereComponent for root
#include "Engine/EngineTypes.h"
#include "GenericTeamAgentInterface.h"
#include "GameFramework/DamageType.h"
#include "Engine/CollisionProfile.h"
#include "Logging/SolaraqLogChannels.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
//#include "Gameplay/Pickups/SolaraqPickupBase.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Projectiles/SolaraqHomingProjectile.h"
#include "Projectiles/SolaraqProjectile.h"
#include "Components/DockingPadComponent.h" // Include for docking logic
#include "TimerManager.h" // For potential future timed sequences
#include "Controllers/SolaraqPlayerController.h"
#include "Core/SolaraqGameInstance.h"
#include "Engine/DamageEvents.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h" 

// Simple Logging Helper Macro
#define NET_LOG(LogCat, Verbosity, Format, ...) \
UE_LOG(LogCat, Verbosity, TEXT("[%s] %s: " Format), \
(GetNetMode() == NM_Client ? TEXT("CLIENT") : (GetNetMode() == NM_ListenServer || GetNetMode() == NM_DedicatedServer ? TEXT("SERVER") : TEXT("STANDALONE"))), \
*FString(__FUNCTION__), \
##__VA_ARGS__)


// Sets default values
ASolaraqShipBase::ASolaraqShipBase()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    CurrentHealth = MaxHealth;
    bIsDead = false;
    
    ThrustForce = 1400000.0f;
    TurnSpeed = 110.0f;
    NormalMaxSpeed = 2300.0f;
    BoostMaxSpeed = 6000.0f; // Ensure this is set
    Dampening = 0.05f;

    CurrentEnergy = MaxEnergy;
    bIsBoosting = false;
    bIsAttemptingBoostInput = false;
    LastBoostStopTime = -1.0f;

    CurrentEffectiveScaleFactor_Server = 1.0f;
    
    CollisionAndPhysicsRoot = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionAndPhysicsRoot"));
    SetRootComponent(CollisionAndPhysicsRoot);
    CollisionAndPhysicsRoot->SetMobility(EComponentMobility::Movable);
    CollisionAndPhysicsRoot->InitSphereRadius(40.f); // Example radius
    CollisionAndPhysicsRoot->SetSimulatePhysics(true);
    CollisionAndPhysicsRoot->SetEnableGravity(false);
    CollisionAndPhysicsRoot->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
    CollisionAndPhysicsRoot->SetNotifyRigidBodyCollision(true);

    if (FBodyInstance* BodyInst = CollisionAndPhysicsRoot->GetBodyInstance())
    {
        BodyInst->bLockZTranslation = true;
        BodyInst->bLockXRotation = true;
        BodyInst->bLockYRotation = true;
        BodyInst->bLockZRotation = false;
        BodyInst->LinearDamping = Dampening;
        BodyInst->AngularDamping = 0.8f;
        UE_LOG(LogSolaraqSystem, Log, TEXT("Physics constraints and damping set for CollisionAndPhysicsRoot on %s"), *GetNameSafe(this));
    }
    else
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("Could not get BodyInstance for CollisionAndPhysicsRoot on %s"), *GetNameSafe(this));
    }
    
    ShipMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
    ShipMeshComponent->SetupAttachment(RootComponent);
    ShipMeshComponent->SetSimulatePhysics(false);
    ShipMeshComponent->SetEnableGravity(false);
    ShipMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
    ShipMeshComponent->SetNotifyRigidBodyCollision(false); // Visual mesh usually doesn't need this

    // Shield Mesh Component
    ShieldMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
    ShieldMeshComponent->SetupAttachment(ShipMeshComponent); // Attach to ship mesh or root
    ShieldMeshComponent->SetVisibility(false); // Initially invisible
    ShieldMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName); // No collision for visual effect
    ShieldMeshComponent->SetSimulatePhysics(false);
    ShieldMeshComponent->SetEnableGravity(false);
    // TODO: Assign a default sphere mesh and a shield material in Blueprint or C++ post-init
    
    MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
    if (MuzzlePoint && ShipMeshComponent)
    {
        MuzzlePoint->SetupAttachment(ShipMeshComponent);
        MuzzlePoint->SetRelativeLocation(FVector(100.0f, 0.0f, 0.0f));
    }
    else if (MuzzlePoint && CollisionAndPhysicsRoot)
    {
        MuzzlePoint->SetupAttachment(CollisionAndPhysicsRoot);
        MuzzlePoint->SetRelativeLocation(FVector(100.0f, 0.0f, 0.0f));
    }

    ProjectileMuzzleSpeed = 8000.0f;
    FireRate = 0.5f;
    LastFireTime = -1.0f;

    LastHomingFireTime = -1.0f; // Initialize homing missile timer
    
    SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArmComponent->SetupAttachment(RootComponent);
    SpringArmComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
    SpringArmComponent->TargetArmLength = 3000.0f;
    SpringArmComponent->bEnableCameraLag = false;
    SpringArmComponent->bEnableCameraRotationLag = false;
    SpringArmComponent->bDoCollisionTest = false;
    SpringArmComponent->bInheritPitch = false;
    SpringArmComponent->bInheritYaw = false;
    SpringArmComponent->bInheritRoll = false;

    bReplicates = true;
    SetReplicateMovement(true);
    if(CollisionAndPhysicsRoot) CollisionAndPhysicsRoot->SetIsReplicated(true); // Replicate physics root state
    
    // Default docking state
    CurrentDockingStatus = EDockingStatus::None;
    ActiveDockingPad = nullptr;

    NetUpdateFrequency = 100.0f; // Default is often 100, but depends on project. Try 30, 60.
    MinNetUpdateFrequency = 30.0f;

    // Shield Defaults
    CurrentShieldEnergy = MaxShieldEnergy;
    bIsShieldActive = false;
    LastShieldDeactivationTime = -1.0f;
    ShieldTimerUpdateInterval = 0.1f;
    MaxShieldStrength = 100.0f; // e.g., 100 HP
    CurrentShieldStrength = 0.0f; 
    
    UE_LOG(LogSolaraqGeneral, Log, TEXT("ASolaraqShipBase %s Constructed"), *GetName());
}

void ASolaraqShipBase::Client_SetVisualScale_Implementation(float NewScaleFactor)
{
    ApplyVisualScale(NewScaleFactor);
}

void ASolaraqShipBase::Client_ResetVisualScale_Implementation()
{
    ApplyVisualScale(1.0f);
}

float ASolaraqShipBase::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
    class AController* EventInstigator, AActor* DamageCauser)
{
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s: TakeDamage CALLED. DamageAmount: %.1f, DamageCauser: %s, EventInstigator: %s"), 
        *GetNameSafe(this), DamageAmount, *GetNameSafe(DamageCauser), *GetNameSafe(EventInstigator));

    if (bIsDead || DamageAmount <= 0.0f)
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s: TakeDamage returning 0. IsDead: %d, DamageAmount: %.1f"), *GetNameSafe(this), bIsDead, DamageAmount);
        return 0.0f;
    }

    float DamageAppliedToHealth = DamageAmount;

    if (HasAuthority())
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s: TakeDamage HAS AUTHORITY. bIsShieldActive: %d, CurrentShieldEnergy: %.1f"), 
            *GetNameSafe(this), bIsShieldActive, CurrentShieldEnergy);

        if (bIsShieldActive && CurrentShieldEnergy > 0.0f)
        {
            float ShieldStrengthBeforeDamage = CurrentShieldStrength;
            float DamageAbsorbedByShield = FMath::Min(DamageAmount, CurrentShieldStrength);
            CurrentShieldStrength -= DamageAbsorbedByShield;
            DamageAppliedToHealth -= DamageAbsorbedByShield;

            UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s: Shield ACTIVE & HAS STRENGTH. StrengthBefore: %.1f. Absorbed: %.1f. StrengthAfter: %.1f. DamageToHealth: %.1f. DURATION Energy: %.1f (unaffected by this damage event)."),
                *GetNameSafe(this), ShieldStrengthBeforeDamage, DamageAbsorbedByShield, CurrentShieldStrength, DamageAppliedToHealth, CurrentShieldEnergy);

            FVector ImpactLocation = GetActorLocation(); // Default
            if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
            {
                const FPointDamageEvent* PointDamageEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
                ImpactLocation = PointDamageEvent->HitInfo.ImpactPoint;
            }
            else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
            {
                 const FRadialDamageEvent* RadialDamageEvent = static_cast<const FRadialDamageEvent*>(&DamageEvent);
                 ImpactLocation = RadialDamageEvent->Origin;
            }
            UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s: Calling Multicast_PlayShieldImpactEffects at %s for %.1f damage absorbed."),
                *GetNameSafe(this), *ImpactLocation.ToString(), DamageAbsorbedByShield);
            Multicast_PlayShieldImpactEffects(ImpactLocation, DamageAbsorbedByShield);


            if (CurrentShieldStrength <= 0.0f)
            {
                CurrentShieldStrength = 0.0f;
                UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s: Shield STRENGTH depleted (<=0) by damage. Calling Server_DeactivateShield(true, false) (Forced=true, SkipCooldown=false)."), *GetNameSafe(this));
                Server_DeactivateShield(true, false); // Shield broke from damage
            }
            
            if (DamageAppliedToHealth <= 0.0f)
            {
                UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s: Shield absorbed all damage. Returning 0 damage applied to health."), *GetNameSafe(this));
                return 0.0f;
            }
        }

        // Apply remaining damage (if any) to health
        float HealthBeforeDamage = CurrentHealth;
        CurrentHealth = FMath::Clamp(CurrentHealth - DamageAppliedToHealth, 0.0f, MaxHealth);
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s: Applied %.1f damage to health. HealthBefore: %.1f, HealthAfter: %.1f"), 
            *GetNameSafe(this), DamageAppliedToHealth, HealthBeforeDamage, CurrentHealth);

        if (CurrentHealth <= 0.0f)
        {
            UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s: Health depleted (<=0). Calling HandleDestruction()."), *GetNameSafe(this));
            HandleDestruction();
        }
    }
    else
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s: TakeDamage NO AUTHORITY. Returning original DamageAppliedToHealth: %.1f"), *GetNameSafe(this), DamageAppliedToHealth);
    }
    return DamageAppliedToHealth;
}

FRotator ASolaraqShipBase::GetActualDockingTargetRelativeRotation() const
{
    return ActualDockingTargetRelativeRotation;
}

float ASolaraqShipBase::GetHealthPercentage() const
{
    if (MaxHealth <= 0.0f) return 0.0f;
    return CurrentHealth / MaxHealth;
}

void ASolaraqShipBase::BeginPlay()
{
    Super::BeginPlay();

    CurrentEnergy = MaxEnergy;

    if (ShipMeshComponent)
    {
        DefaultVisualMeshScale = ShipMeshComponent->GetRelativeScale3D();
        LastAppliedScaleFactor = 1.0f; // Assuming uniform scale initially for simplicity
        if (DefaultVisualMeshScale.IsUniform())
        {
            LastAppliedScaleFactor = DefaultVisualMeshScale.X;
        } else {
            UE_LOG(LogSolaraqCelestials, Warning, TEXT("Ship %s has non-uniform default scale. Scale factor RPC might be slightly inaccurate. Resetting internal default to 1,1,1."), *GetName());
            DefaultVisualMeshScale = FVector::OneVector; // Reset to uniform for calculations
            LastAppliedScaleFactor = 1.0f;
        }
    }

    if (HasAuthority())
    {
        CurrentHealth = MaxHealth;
        CurrentShieldEnergy = MaxShieldEnergy;
        bIsDead = false;

        FTimerHandle DummyTimerHandle; 
        float DelayDuration = 0.2f; // Or your chosen delay
        GetWorldTimerManager().SetTimer(DummyTimerHandle, this, &ASolaraqShipBase::Server_AttemptReestablishDockingAfterLoad, DelayDuration, false);
        
        float CurrentTime = GetWorld() ? GetWorld()->TimeSeconds : -1.f;
        UE_LOG(LogSolaraqTransition, Warning, TEXT("Ship %s BeginPlay: Scheduled Server_AttemptReestablishDockingAfterLoad with %.2fs delay. Current Time: %.2f"),
            *GetName(), DelayDuration, CurrentTime);
    }
    
    UE_LOG(LogSolaraqGeneral, Log, TEXT("ASolaraqShipBase %s BeginPlay called."), *GetName());
}

void ASolaraqShipBase::ApplyVisualScale(float ScaleFactor)
{
    if (!FMath::IsNearlyEqual(ScaleFactor, LastAppliedScaleFactor, 0.01f))
    {
        if (ShipMeshComponent)
        {
            ShipMeshComponent->SetRelativeScale3D(DefaultVisualMeshScale * ScaleFactor);
            LastAppliedScaleFactor = ScaleFactor;
        }
    }
}

void ASolaraqShipBase::Server_RequestFireHomingMissileAtTarget_Implementation(AActor* TargetToShootAt)
{
    if (!TargetToShootAt || TargetToShootAt->IsPendingKillPending())
    {
        NET_LOG(LogSolaraqCombat, Warning, TEXT("Server received fire request with invalid target %s. Ignoring."), *GetNameSafe(TargetToShootAt));
        return;
    }
    PerformFireHomingMissile(TargetToShootAt);
}

void ASolaraqShipBase::PerformFireHomingMissile(AActor* HomingTarget) 
{
    if (!HasAuthority() || IsDead() || !HomingProjectileClass || IsShipDockedOrDocking() || bIsUnderScalingEffect_Server) return;

    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    if (CurrentTime < LastHomingFireTime + HomingMissileFireRate)
    {
        return; // Cooldown
    }

    if (!MuzzlePoint) { UE_LOG(LogSolaraqCombat, Error, TEXT("%s PerformFireHomingMissile: MuzzlePoint is NULL!"), *GetName()); return; }
    UWorld* const World = GetWorld();
    if (!World) { UE_LOG(LogSolaraqCombat, Error, TEXT("%s PerformFireHomingMissile: GetWorld() is NULL!"), *GetName()); return; }

    if (!HomingTarget)
    {
        NET_LOG(LogSolaraqCombat, Error, TEXT("%s PerformFireHomingMissile called with NULL target!"), *GetName());
        return;
    }

    const FVector MuzzleLocation = MuzzlePoint->GetComponentLocation();
    const FRotator MuzzleRotation = MuzzlePoint->GetComponentRotation();

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ASolaraqHomingProjectile* SpawnedMissile = World->SpawnActor<ASolaraqHomingProjectile>(
        HomingProjectileClass, MuzzleLocation, MuzzleRotation, SpawnParams
    );

    if (SpawnedMissile)
    {
        SpawnedMissile->SetupHomingTarget(HomingTarget);

        UProjectileMovementComponent* ProjMoveComp = SpawnedMissile->GetProjectileMovement();
        if(ProjMoveComp) {
             const FVector MuzzleForward = MuzzleRotation.Vector();
             const FVector ShipVelocity = CollisionAndPhysicsRoot ? CollisionAndPhysicsRoot->GetPhysicsLinearVelocity() : FVector::ZeroVector;
             ProjMoveComp->InitialSpeed = HomingMissileLaunchSpeed; // Set InitialSpeed on PMC
             ProjMoveComp->Velocity = ShipVelocity + (MuzzleForward * HomingMissileLaunchSpeed); // Or set Velocity directly
             ProjMoveComp->Activate(); // Ensure it's active
             //ProjMoveComp->UpdateComponentVelocity(); // May not be needed if InitialSpeed is used and component activates
        }

        NET_LOG(LogSolaraqCombat, Log, TEXT("Fired Homing Missile %s at %s (LaunchSpeed: %.1f)"),
            *SpawnedMissile->GetName(), *HomingTarget->GetName(), HomingMissileLaunchSpeed);
        LastHomingFireTime = CurrentTime;
    }
    else
    {
        UE_LOG(LogSolaraqProjectile, Error, TEXT("%s PerformFireHomingMissile: Failed to spawn HomingProjectile!"), *GetName());
    }
}


void ASolaraqShipBase::Server_RequestToggleShield_Implementation()
{
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Server_RequestToggleShield_Implementation CALLED."), *GetNameSafe(this));
    if (!HasAuthority() || IsDead() || IsShipDockedOrDocking())
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Cannot toggle shield. HasAuthority: %d, IsDead: %d, IsDockedOrDocking: %d."),
            *GetNameSafe(this), HasAuthority(), IsDead(), IsShipDockedOrDocking());
        return;
    }

    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): CurrentTime: %.2f. bIsShieldActive: %d."), *GetNameSafe(this), CurrentTime, bIsShieldActive);

    if (bIsShieldActive)
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Shield is ACTIVE. Calling Server_DeactivateShield(false)."), *GetNameSafe(this));
        Server_DeactivateShield(false); 
    }
    else
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Shield is INACTIVE. LastShieldDeactivationTime: %.2f, ShieldActivationCooldown: %.2f."), 
            *GetNameSafe(this), LastShieldDeactivationTime, ShieldActivationCooldown);
        if (LastShieldDeactivationTime > 0.f && CurrentTime < LastShieldDeactivationTime + ShieldActivationCooldown)
        {
            UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Cannot activate shield: Still in activation cooldown. Time remaining: %.1fs"),
                *GetNameSafe(this), (LastShieldDeactivationTime + ShieldActivationCooldown) - CurrentTime);
            return;
        }
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): CurrentShieldEnergy: %.1f, MinEnergyToActivateShield: %.1f."),
            *GetNameSafe(this), CurrentShieldEnergy, MinEnergyToActivateShield);
        if (CurrentShieldEnergy < MinEnergyToActivateShield) // Check DURATION energy
        {
            UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Cannot activate shield: Not enough DURATION energy (%.1f / %.1f required)."),
                *GetNameSafe(this), CurrentShieldEnergy, MinEnergyToActivateShield);
            return;
        }
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Shield is INACTIVE and conditions met. Calling Server_ActivateShield()."), *GetNameSafe(this));
        Server_ActivateShield();
    }
}

void ASolaraqShipBase::Server_AttemptReestablishDockingAfterLoad()
{
    if (!HasAuthority()) return;

    float CurrentTime = GetWorld() ? GetWorld()->TimeSeconds : -1.f;
    UE_LOG(LogSolaraqTransition, Warning, TEXT("Ship %s: Server_AttemptReestablishDockingAfterLoad called. World: %s, Time: %.2f"),
        *GetName(), *GetWorld()->GetName(), CurrentTime);
    
    USolaraqGameInstance* GI = GetGameInstance<USolaraqGameInstance>();
    if (!GI)
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("  GI is null. Cannot re-establish docking."), *GetName());
        return;
    }

    // Check if this ship is the one intended for re-docking
    // And if there's a target pad ID
    if (GI->PlayerShipNameInOriginLevel != GetFName() || GI->DockingPadIdentifierToReturnTo == NAME_None)
    {
        if (GI->PlayerShipNameInOriginLevel != GetFName())
        {
            UE_LOG(LogSolaraqSystem, Log, TEXT("  This ship (%s) is not the target ship in GI (%s). Skipping re-dock."), *GetFName().ToString(), *GI->PlayerShipNameInOriginLevel.ToString());
        }
        if (GI->DockingPadIdentifierToReturnTo == NAME_None)
        {
            UE_LOG(LogSolaraqSystem, Log, TEXT("  DockingPadIdentifierToReturnTo in GI is None. Skipping re-dock."));
        }
        // GI->ClearTransitionData(); // Optionally clear GI data if this ship is not the one, or it's done trying.
        return;
    }

    UE_LOG(LogSolaraqTransition, Warning, TEXT("  This ship (%s) HAS 'PlayerShip_Persistent' tag. Looking for Pad ID: %s. Time: %.2f"),
        *GetName(), *GI->DockingPadIdentifierToReturnTo.ToString(), CurrentTime);
    
    UDockingPadComponent* TargetPad = nullptr;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It) // Iterate all actors
    {
        AActor* FoundActor = *It;
        // Find all DockingPadComponents on each actor
        TArray<UDockingPadComponent*> PadsOnActor;
        FoundActor->GetComponents<UDockingPadComponent>(PadsOnActor);
        for (UDockingPadComponent* PadComponent : PadsOnActor)
        {
            if (PadComponent && PadComponent->DockingPadUniqueID == GI->DockingPadIdentifierToReturnTo)
            {
                TargetPad = PadComponent;
                UE_LOG(LogSolaraqSystem, Log, TEXT("  Found matching DockingPadComponent: %s on Actor %s"), *TargetPad->GetName(), *FoundActor->GetName());
                break; 
            }
        }
        if (TargetPad) break;
    }

    if (!TargetPad)
    {
        UE_LOG(LogSolaraqTransition, Warning, TEXT("  Could not find DockingPadComponent with ID %s in the world. Time: %.2f"),
            *GI->DockingPadIdentifierToReturnTo.ToString(), CurrentTime);// Clear data as we can't use it
        return;
    }

    if (!TargetPad->IsPadFree_Server())
    {
        ASolaraqShipBase* OccupyingShip = TargetPad->GetOccupyingShip_Server();
        UE_LOG(LogSolaraqSystem, Warning, TEXT("  Target Pad %s is not free. Currently occupied by %s. Cannot re-dock this ship (%s)."),
            *TargetPad->GetName(), *GetNameSafe(OccupyingShip), *GetName());
        GI->ClearTransitionData(); // Clear data
        return;
    }

    // If we reach here, we are this ship, found the target pad, and it's free.
    UE_LOG(LogSolaraqSystem, Log, TEXT("  Target Pad %s is free. Attempting to re-dock ship %s."), *TargetPad->GetName(), *GetName());

    // If the ship is already somehow docked (e.g. persistence worked perfectly), just ensure state.
    // Or if it's very close to the pad, we might just snap and set docked state without full lerp.
    // For now, let's assume it might be anywhere (e.g., spawned at PlayerStart if re-possess failed initially).

    // Force undock if by some miracle it was docked to something else or in a weird state.
    if (IsShipDockedOrDocking())
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("  Ship %s was already in a docking state (%s with %s). Forcing undock before re-docking to target pad %s."),
            *GetName(), *UEnum::GetValueAsString(CurrentDockingStatus), *GetNameSafe(ActiveDockingPad), *TargetPad->GetName());
        Server_RequestUndock(); // This will clear ActiveDockingPad and set status to None.
    }
    
    // Move the ship to the docking pad's attach point
    // This is a hard snap; alternatively, you could initiate a short lerp.
    if (TargetPad->GetAttachPoint())
    {
        FRotator RelativeRotationToApply = FRotator::ZeroRotator; // Default
        if (GI)
        {
            RelativeRotationToApply = GI->ShipDockedRelativeRotation;
            UE_LOG(LogSolaraqSystem, Log, TEXT("  Using saved relative rotation from GI: %s"), *RelativeRotationToApply.ToString());
        } else {
            UE_LOG(LogSolaraqSystem, Warning, TEXT("  GI is NULL when trying to get saved rotation. Defaulting to ZeroRotator."));
        }


        FTransform TargetPadAttachTransform = TargetPad->GetAttachPoint()->GetComponentTransform();
        
        // Calculate the desired WORLD rotation based on the pad's world rotation and the saved relative rotation
        FQuat PadWorldQuat = TargetPadAttachTransform.GetRotation();
        FQuat RelativeQuat = FQuat(RelativeRotationToApply);
        FQuat FinalWorldQuat = PadWorldQuat * RelativeQuat; // Apply relative to pad's orientation
        FRotator FinalWorldRotation = FinalWorldQuat.Rotator();

        // Location snapping (DockingTargetRelativeLocation is usually ZeroVector for direct attach point docking)
        FVector FinalWorldLocation = TargetPadAttachTransform.TransformPosition(DockingTargetRelativeLocation);

        SetActorLocationAndRotation(FinalWorldLocation, FinalWorldRotation);
        UE_LOG(LogSolaraqSystem, Log, TEXT("  Snapped ship %s to Pad %s location. Applied world rotation: %s (from relative: %s)"),
            *GetName(), *TargetPad->GetName(), *FinalWorldRotation.ToString(), *RelativeRotationToApply.ToString());
    }
    else
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("  Target Pad %s has no valid AttachPoint component! Cannot snap location."), *TargetPad->GetName());
        GI->ClearTransitionData();
        return;
    }

    // Now, formally request docking with this pad.
    // This will handle physics, attachment, status updates, etc.
    Server_RequestDockWithPad(TargetPad);

    // Successfully used the data, clear it from GI
    GI->ClearTransitionData();
    UE_LOG(LogSolaraqSystem, Log, TEXT("  Re-docking process initiated for ship %s with pad %s. GI transition data cleared."), *GetName(), *TargetPad->GetName());
}

void ASolaraqShipBase::Server_SetAttemptingBoost_Implementation(bool bAttempting)
{
    bIsAttemptingBoostInput = bAttempting;
}

void ASolaraqShipBase::ProcessMoveForwardInput(float Value)
{
    if (HasAuthority())
    {
        if (CollisionAndPhysicsRoot && FMath::Abs(Value) > KINDA_SMALL_NUMBER && !IsShipDockedOrDocking())
        {
            float BaseThrust = bIsBoosting ? (ThrustForce * BoostThrustMultiplier) : ThrustForce;

            // CONFIGURABLE: This should match the effective minimum scale output by CelestialBodyBase.
            // For example, if CelestialBodyBase.MinShipScaleFactor is 0.2 and it uses FMath::Max(MinShipScaleFactor, 0.01f),
            // then this value should be 0.2. If MinShipScaleFactor is 0.0, then this should be 0.01.
            const float MinEffectiveScaleAtFullReduction = 0.1f; // <<<< ENSURE THIS VALUE matches your CelestialBodyBase's actual minimum output scale.

            float ThrustScaleMultiplier = 1.0f;
            float AlphaForThrust = 1.0f; // Default to 1.0 if no scaling applied or if scale is 1.0

            // --- Start Logging Block (Optional: Can be commented out for performance once verified) ---
            /*
            UE_LOG(LogSolaraqMovement, Log, TEXT("THRUST CALC START --- Ship: %s, InputValue: %.2f"), *GetNameSafe(this), Value);
            UE_LOG(LogSolaraqMovement, Log, TEXT("  CurrentEffectiveScale_Server: %.4f"), CurrentEffectiveScaleFactor_Server);
            UE_LOG(LogSolaraqMovement, Log, TEXT("  MinEffectiveScaleAtFullReduction (Hardcoded): %.4f"), MinEffectiveScaleAtFullReduction);
            UE_LOG(LogSolaraqMovement, Log, TEXT("  MinScaleThrustReductionFactor (Ship UPROPERTY): %.4f"), MinScaleThrustReductionFactor);
            */
            // --- End Logging Block ---

            
            if (CurrentEffectiveScaleFactor_Server < 1.0f && CurrentEffectiveScaleFactor_Server >= MinEffectiveScaleAtFullReduction)
            {
                // --- Logging for Condition Met ---
                /*
                UE_LOG(LogSolaraqMovement, Log, TEXT("  Condition Met: Scale (%.4f) is < 1.0 and >= MinEffectiveScale (%.4f)"), CurrentEffectiveScaleFactor_Server, MinEffectiveScaleAtFullReduction);
                */

                // CORRECTED FMath::GetRangePct call: (MinValue, MaxValue, Value)
                AlphaForThrust = FMath::GetRangePct(MinEffectiveScaleAtFullReduction, 1.0f, CurrentEffectiveScaleFactor_Server);
                
                // Clamp alpha: GetRangePct can return outside [0,1] if Value is outside [MinValue,MaxValue].
                // Our surrounding 'if' condition aims to keep Value within this range for this block,
                // but clamping is a good safeguard for Lerp.
                AlphaForThrust = FMath::Clamp(AlphaForThrust, 0.0f, 1.0f);

                // --- Logging for Alpha and Lerp ---
                /*
                UE_LOG(LogSolaraqMovement, Log, TEXT("    GetRangePct(Min=%.4f, Max=1.0f, Value=%.4f) -> Clamped AlphaForThrust = %.4f"),
                    MinEffectiveScaleAtFullReduction, CurrentEffectiveScaleFactor_Server, AlphaForThrust);
                */

                // Lerp between MinScaleThrustReductionFactor (when Alpha is 0) and 1.0 (when Alpha is 1).
                ThrustScaleMultiplier = FMath::Lerp(MinScaleThrustReductionFactor, 1.0f, AlphaForThrust);
                
                /*
                UE_LOG(LogSolaraqMovement, Log, TEXT("    Lerp(A=%.4f, B=1.0f, Alpha=%.4f) -> ThrustScaleMultiplier = %.4f"),
                    MinScaleThrustReductionFactor, AlphaForThrust, ThrustScaleMultiplier);
                */
            }
            else if (CurrentEffectiveScaleFactor_Server < MinEffectiveScaleAtFullReduction)
            {
                // --- Logging for Scale Below Minimum ---
                /*
                UE_LOG(LogSolaraqMovement, Log, TEXT("  Condition Met: Scale (%.4f) is < MinEffectiveScale (%.4f). Using MinScaleThrustReductionFactor directly."), CurrentEffectiveScaleFactor_Server, MinEffectiveScaleAtFullReduction);
                */
                ThrustScaleMultiplier = MinScaleThrustReductionFactor;
                AlphaForThrust = 0.0f; // Conceptually, Alpha is 0 here
            }
            else // Scale is 1.0f or greater (or an unexpected state)
            {
                // --- Logging for No Reduction ---
                /*
                 UE_LOG(LogSolaraqMovement, Log, TEXT("  Condition NOT Met for reduction: Scale (%.4f) is 1.0 or not in reduction range. ThrustScaleMultiplier remains 1.0."), CurrentEffectiveScaleFactor_Server);
                */
                 ThrustScaleMultiplier = 1.0f; // Ensure it's 1.0 if no reduction conditions met
                 AlphaForThrust = 1.0f;
            }

            const float ActualThrust = BaseThrust * ThrustScaleMultiplier;
            const FVector ForceDirection = GetActorForwardVector();
            const FVector ForceToAdd = ForceDirection * Value * ActualThrust;

            //NET_LOG(LogSolaraqMovement, Warning, TEXT("ProcessMoveForwardInput (Server): Value: %.2f, bIsBoosting: %d, ThrustForce: %.2f, BoostMult: %.2f, ThrustScaleMult: %.4f"), Value, bIsBoosting, ThrustForce, BoostThrustMultiplier, ThrustScaleMultiplier);
            FVector CurrentVel = CollisionAndPhysicsRoot ? CollisionAndPhysicsRoot->GetPhysicsLinearVelocity() : FVector::ZeroVector;
            //NET_LOG(LogSolaraqMovement, Warning, TEXT("ProcessMoveForwardInput (Server): ForceToAdd: %s, CurrentVel Before: %s"), *ForceToAdd.ToString(), *CurrentVel.ToString());
            
            CollisionAndPhysicsRoot->AddForce(ForceToAdd, NAME_None, false);

            CurrentVel = CollisionAndPhysicsRoot ? CollisionAndPhysicsRoot->GetPhysicsLinearVelocity() : FVector::ZeroVector;
            //NET_LOG(LogSolaraqMovement, Warning, TEXT("ProcessMoveForwardInput (Server): CurrentVel After AddForce: %s"), *CurrentVel.ToString());
            
            // --- Final Thrust Logging ---
            /*
            UE_LOG(LogSolaraqMovement, Log, TEXT("  BaseThrust: %.2f, Final ThrustScaleMultiplier: %.4f, ActualThrustApplied: %.2f"),
                BaseThrust, ThrustScaleMultiplier, ActualThrust);
            UE_LOG(LogSolaraqMovement, Log, TEXT("THRUST CALC END ---"));
            */
        }
    }
}

void ASolaraqShipBase::ProcessTurnInput(float Value)
{
    if (HasAuthority())
    {
        if (FMath::Abs(Value) > KINDA_SMALL_NUMBER && !IsShipDockedOrDocking())
        {
            const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
            const float RotationThisFrame = Value * TurnSpeed * DeltaSeconds;
            AddActorLocalRotation(FRotator(0.0f, RotationThisFrame, 0.0f));
        }
    }
}

void ASolaraqShipBase::Server_SendMoveForwardInput_Implementation(float Value)
{
    // This log should appear on the SERVER instance of the ship
    NET_LOG(LogSolaraqMovement, Warning, TEXT("SERVER SHIP %s: Server_SendMoveForwardInput_Implementation RECEIVED Value: %.2f. Current Authority: %d, IsLocallyControlled: %d"),
        *GetNameSafe(this), Value, HasAuthority(), IsLocallyControlled());
    
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    
    // This function now executes ON THE SERVER, called by a client
    
    // Check if docked and player is trying to move forward
    if ((IsShipDocked() || CurrentDockingStatus == EDockingStatus::Docking) && (Value > 0.1f || Value < -0.1f))// Value > 0.1f to ensure it's a deliberate forward thrust
    {
        // Check Undock Grace Period: Can we undock via thrust yet?
        if (CurrentDockingStartTime > 0.f && CurrentTime < CurrentDockingStartTime + UndockFromThrustGracePeriod)
        {
            //NET_LOG(LogSolaraqInput, Verbose, TEXT("Server_SendMoveForwardInput: Forward thrust received while docked, but still in undock grace period. Ignoring for undock. Time remaining: %.1fs"), (CurrentDockingStartTime + UndockFromThrustGracePeriod) - CurrentTime);
            // Do not process as undock, and also don't process as movement (ship is docked)
            return;
        }
        
        // If docked and trying to move forward, interpret as an undock request
        //NET_LOG(LogSolaraqInput, Log, TEXT("Server_SendMoveForwardInput: Ship is docked/docking and received forward thrust (Value: %.2f). Requesting undock."), Value);
        Server_RequestUndock(); // Initiate undocking
        // Do not process movement input this frame as we are undocking.
        return; 
    }

    // If already undocking, or in a state where movement is disallowed, or not trying to undock via thrust
    if (IsShipDockedOrDocking()) // This will catch 'AttemptingDock', 'Docking', 'Docked' and the bIsLerpingToDockPosition flag
    {
        // UE_LOG(LogSolaraqMovement, Verbose, TEXT("Ship %s: Move input ignored, currently docked/docking/lerping."), *GetName());
        return;
    }

    // If not docked and not trying to undock via thrust, process normal movement
    ProcessMoveForwardInput(Value);
}

void ASolaraqShipBase::Server_SendTurnInput_Implementation(float Value)
{
    if (IsShipDockedOrDocking()) return;
    ProcessTurnInput(Value);
    SetTurnInputForRoll(Value);
}

void ASolaraqShipBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (ShipMeshComponent)
    {
        float TargetRollAngleThisFrame;
        if (IsShipDockedOrDocking() || bIsLerpingToDockPosition) // If docked, docking, or lerping to dock
        {
            // When docked or in the process of docking, always target zero roll (level)
            TargetRollAngleThisFrame = 0.0f;
        }
        else
        {
            // Otherwise, use the turn input for roll
            TargetRollAngleThisFrame = CurrentTurnInputForRoll * MaxTurnRollAngle;
        }
        
        CurrentVisualRoll = FMath::FInterpTo(CurrentVisualRoll, TargetRollAngleThisFrame, DeltaTime, RollInterpolationSpeed);
        
        FRotator CurrentMeshRelativeRotation = ShipMeshComponent->GetRelativeRotation();
        ShipMeshComponent->SetRelativeRotation(FRotator(
            CurrentMeshRelativeRotation.Pitch,
            CurrentMeshRelativeRotation.Yaw, // Keep current relative Yaw of the mesh
            CurrentVisualRoll                // Apply interpolated Roll
        ));
    }
    
    if (HasAuthority())
    {
        // --- Docking Lerp Logic (Server-Side) ---
        if (bIsLerpingToDockPosition && GetRootComponent() && LerpAttachTargetComponent && GetRootComponent()->GetAttachParent() == LerpAttachTargetComponent)
        {
            const FVector CurrentRelativeLocation = GetRootComponent()->GetRelativeLocation();
            const FRotator CurrentRelativeRotation = GetRootComponent()->GetRelativeRotation();

            const FVector NewRelativeLocation = FMath::VInterpTo(CurrentRelativeLocation, DockingTargetRelativeLocation, DeltaTime, DockingLerpSpeed);
            // Use the dynamically calculated ActualDockingTargetRelativeRotation
            const FRotator NewRelativeRotation = FMath::RInterpTo(CurrentRelativeRotation, ActualDockingTargetRelativeRotation, DeltaTime, DockingLerpSpeed);

            GetRootComponent()->SetRelativeLocationAndRotation(NewRelativeLocation, NewRelativeRotation);

            // Check if lerp is complete
            if (FVector::DistSquared(NewRelativeLocation, DockingTargetRelativeLocation) < KINDA_SMALL_NUMBER * KINDA_SMALL_NUMBER &&
                NewRelativeRotation.Equals(ActualDockingTargetRelativeRotation, 1.0f)) // Compare against Actual
            {
                bIsLerpingToDockPosition = false;
                LerpAttachTargetComponent = nullptr; 
                CurrentDockingStatus = EDockingStatus::Docked; 
                Internal_DisableSystemsForDocking(); 

                NET_LOG(LogSolaraqSystem, Log, TEXT("Ship %s finished lerping to dock position. Status: Docked. Final RelRot: %s"), *GetName(), *NewRelativeRotation.ToString());

                USolaraqGameInstance* GI = GetGameInstance<USolaraqGameInstance>();
                if (GI && ActiveDockingPad) // Ensure we have an active pad to associate this rotation with
                {
                    GI->ShipDockedRelativeRotation = ActualDockingTargetRelativeRotation; // This is already relative
                    UE_LOG(LogSolaraqSystem, Log, TEXT("Ship %s: Saved docked relative rotation %s to GameInstance."),
                        *GetName(), *ActualDockingTargetRelativeRotation.ToString());
                }
                
                OnRep_DockingStateChanged();
                
            }
        }
        // --- End Docking Lerp Logic ---

        // If we just completed docking, ensure CurrentTurnInputForRoll is cleared on server
        // This is mostly for AI that might have set it. Player input would naturally stop.
        if (CurrentDockingStatus == EDockingStatus::Docked && !bIsLerpingToDockPosition)
        {
            if (CurrentTurnInputForRoll != 0.0f)
            {
                CurrentTurnInputForRoll = 0.0f; // This will replicate and help clients snap roll to 0 if needed
            }
        }
        
        // ... (existing boost logic, make sure it checks !IsShipDockedOrDocking() and !bIsLerpingToDockPosition) ...
        if (!IsShipDockedOrDocking() && !bIsLerpingToDockPosition) // Cannot boost if docked or lerping
        {
            // ... (boost energy drain/regen) ...
        }
        else // If docked or lerping, ensure boosting is off
        {
             if (bIsBoosting || bIsAttemptingBoostInput)
             {
                 bIsAttemptingBoostInput = false; 
                 bIsBoosting = false; 
                 LastBoostStopTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
             }
        }
        
        // ... (existing velocity clamping logic) ...
        if (!IsShipDockedOrDocking() && !bIsLerpingToDockPosition && CollisionAndPhysicsRoot && CollisionAndPhysicsRoot->IsSimulatingPhysics())
        {
            ClampVelocity();
        }
    }
    // ... (client-side tick logic, clamping etc. needs to respect docking state too) ...
     if (!IsShipDockedOrDocking() && !bIsLerpingToDockPosition && CollisionAndPhysicsRoot && CollisionAndPhysicsRoot->IsSimulatingPhysics())
    {
       // ClampVelocity(); // Client-side clamping, use with caution
    }

    if (GetNetMode() == NM_Client && CollisionAndPhysicsRoot) { // THIS BLOCK
        FVector ClientVel = CollisionAndPhysicsRoot->GetPhysicsLinearVelocity();
        bool bIsSimulatingPhysics = CollisionAndPhysicsRoot->IsSimulatingPhysics();
        EDockingStatus ClientDockStatus = CurrentDockingStatus;
        bool ClientIsDead = bIsDead;

        // MAKE SURE THIS LINE IS UNCOMMENTED AND USING A VISIBLE VERBOSITY
        /*NET_LOG(LogSolaraqMovement, Warning, TEXT("CLIENT TICK: Vel: %s (Speed: %.2f), SimPhys: %d, DockStatus: %s, IsDead: %d, ActorLoc: %s"),
            *ClientVel.ToString(),
            ClientVel.Size(),
            bIsSimulatingPhysics,
            *UEnum::GetValueAsString(ClientDockStatus),
            ClientIsDead,
            *GetActorLocation().ToString());*/
    }
    
    // Useful for debugging in PIE.
    if ( (IsLocallyControlled() && GetNetMode() != NM_DedicatedServer) || (HasAuthority() && GetNetMode() != NM_Client) )
    {
        FString ShieldDebugText = FString::Printf(TEXT("--- SHIELD STATUS [%s] ---"), *GetNameSafe(this));
        ShieldDebugText += FString::Printf(TEXT("\nIsActive: %s"), bIsShieldActive ? TEXT("TRUE") : TEXT("FALSE"));
        ShieldDebugText += FString::Printf(TEXT("\nEnergy: %.1f / %.1f"), CurrentShieldEnergy, MaxShieldEnergy);
        ShieldDebugText += FString::Printf(TEXT("\nHP (Strength): %.1f / %.1f"), CurrentShieldStrength, MaxShieldStrength);
        
        float WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.f;
        ShieldDebugText += FString::Printf(TEXT("\nLastDeactTime: %.2f (World: %.2f)"), LastShieldDeactivationTime, WorldTime);
        
        if (GetWorld()) // Ensure GetWorld() is valid before accessing TimerManager
        {
            const FTimerManager& TM = GetWorldTimerManager();
            bool bDrainTimerActive = TM.IsTimerActive(TimerHandle_ShieldDrain);
            bool bRegenDelayTimerActive = TM.IsTimerActive(TimerHandle_ShieldRegenDelayCheck);
            bool bRegenTimerActive = TM.IsTimerActive(TimerHandle_ShieldRegen);

            ShieldDebugText += FString::Printf(TEXT("\nDrain Timer: %s"), bDrainTimerActive ? TEXT("Active") : TEXT("Inactive"));
            if(bDrainTimerActive) ShieldDebugText += FString::Printf(TEXT(" (Rem: %.2fs)"), TM.GetTimerRemaining(TimerHandle_ShieldDrain));

            ShieldDebugText += FString::Printf(TEXT("\nRegenDelay Timer: %s"), bRegenDelayTimerActive ? TEXT("Active") : TEXT("Inactive"));
            if(bRegenDelayTimerActive) ShieldDebugText += FString::Printf(TEXT(" (Rem: %.2fs)"), TM.GetTimerRemaining(TimerHandle_ShieldRegenDelayCheck));
            
            ShieldDebugText += FString::Printf(TEXT("\nRegen Timer: %s"), bRegenTimerActive ? TEXT("Active") : TEXT("Inactive"));
            if(bRegenTimerActive) ShieldDebugText += FString::Printf(TEXT(" (Rem: %.2fs)"), TM.GetTimerRemaining(TimerHandle_ShieldRegen));
        }
        else
        {
            ShieldDebugText += TEXT("\nTimerManager: GetWorld() is NULL!");
        }

        // Display slightly above the ship's root component
        FVector TextLocation = GetActorLocation() + FVector(0, 0, 200.0f); // Adjust Z offset if needed
        DrawDebugString(GetWorld(), TextLocation, ShieldDebugText, nullptr, FColor::White, 0.0f, true, 1.0f); // true for shadow, 1.0f for scale
    }
    // --- End Shield Debug On-Screen Text ---
}

void ASolaraqShipBase::ClampVelocity()
{
    if (!CollisionAndPhysicsRoot || !CollisionAndPhysicsRoot->IsSimulatingPhysics())
    {
        return;
    }

    float BaseMaxSpeed = bIsBoosting ? BoostMaxSpeed : NormalMaxSpeed;

    // CONFIGURABLE: This should match the effective minimum scale output by CelestialBodyBase.
    const float MinEffectiveScaleAtFullReduction = 0.1f; // <<<< ENSURE THIS VALUE matches your CelestialBodyBase's actual minimum output scale.

    float SpeedScaleMultiplier = 1.0f;
    float AlphaForSpeed = 1.0f; // Default to 1.0

    // --- Start Logging Block (Optional) ---
    /*
    UE_LOG(LogSolaraqMovement, Log, TEXT("SPEED CALC START --- Ship: %s"), *GetNameSafe(this));
    UE_LOG(LogSolaraqMovement, Log, TEXT("  CurrentEffectiveScale_Server: %.4f"), CurrentEffectiveScaleFactor_Server);
    UE_LOG(LogSolaraqMovement, Log, TEXT("  MinEffectiveScaleAtFullReduction (Hardcoded): %.4f"), MinEffectiveScaleAtFullReduction);
    UE_LOG(LogSolaraqMovement, Log, TEXT("  MinScaleSpeedReductionFactor (Ship UPROPERTY): %.4f"), MinScaleSpeedReductionFactor);
    */
    // --- End Logging Block ---

    if (CurrentEffectiveScaleFactor_Server < 1.0f && CurrentEffectiveScaleFactor_Server >= MinEffectiveScaleAtFullReduction)
    {
        // --- Logging for Condition Met ---
        /*
        UE_LOG(LogSolaraqMovement, Log, TEXT("  Condition Met: Scale (%.4f) is < 1.0 and >= MinEffectiveScale (%.4f)"), CurrentEffectiveScaleFactor_Server, MinEffectiveScaleAtFullReduction);
        */

        // CORRECTED FMath::GetRangePct call: (MinValue, MaxValue, Value)
        AlphaForSpeed = FMath::GetRangePct(MinEffectiveScaleAtFullReduction, 1.0f, CurrentEffectiveScaleFactor_Server);
        
        // Clamp alpha
        AlphaForSpeed = FMath::Clamp(AlphaForSpeed, 0.0f, 1.0f);
        
        // --- Logging for Alpha and Lerp ---
        /*
        UE_LOG(LogSolaraqMovement, Log, TEXT("    GetRangePct(Min=%.4f, Max=1.0f, Value=%.4f) -> Clamped AlphaForSpeed = %.4f"),
            MinEffectiveScaleAtFullReduction, CurrentEffectiveScaleFactor_Server, AlphaForSpeed);
        */
        
        SpeedScaleMultiplier = FMath::Lerp(MinScaleSpeedReductionFactor, 1.0f, AlphaForSpeed);
        /*
        UE_LOG(LogSolaraqMovement, Log, TEXT("    Lerp(A=%.4f, B=1.0f, Alpha=%.4f) -> SpeedScaleMultiplier = %.4f"),
            MinScaleSpeedReductionFactor, AlphaForSpeed, SpeedScaleMultiplier);
        */
    }
    else if (CurrentEffectiveScaleFactor_Server < MinEffectiveScaleAtFullReduction)
    {
        // --- Logging for Scale Below Minimum ---
        /*
        UE_LOG(LogSolaraqMovement, Log, TEXT("  Condition Met: Scale (%.4f) is < MinEffectiveScale (%.4f). Using MinScaleSpeedReductionFactor directly."), CurrentEffectiveScaleFactor_Server, MinEffectiveScaleAtFullReduction);
        */
        SpeedScaleMultiplier = MinScaleSpeedReductionFactor;
        AlphaForSpeed = 0.0f; // Conceptually, Alpha is 0 here
    }
    else // Scale is 1.0f or greater
    {
        // --- Logging for No Reduction ---
        /*
        UE_LOG(LogSolaraqMovement, Log, TEXT("  Condition NOT Met for reduction: Scale (%.4f) is 1.0 or not in reduction range. SpeedScaleMultiplier remains 1.0."), CurrentEffectiveScaleFactor_Server);
        */
        SpeedScaleMultiplier = 1.0f; // Ensure it's 1.0
        AlphaForSpeed = 1.0f;
    }
    
    const float CurrentMaxSpeed = BaseMaxSpeed * SpeedScaleMultiplier;
    const float MaxSpeedSq = FMath::Square(CurrentMaxSpeed);
    
    const FVector CurrentVelocity = CollisionAndPhysicsRoot->GetPhysicsLinearVelocity();
    const float CurrentSpeedSq = CurrentVelocity.SizeSquared();
    
    if (CurrentSpeedSq > MaxSpeedSq)
    {
        const FVector ClampedVelocity = CurrentVelocity.GetSafeNormal() * CurrentMaxSpeed;
        CollisionAndPhysicsRoot->SetPhysicsLinearVelocity(ClampedVelocity);
        // --- Logging for Clamping ---
        /*
        UE_LOG(LogSolaraqMovement, Log, TEXT("  Velocity Clamped: From %.2f To %.2f"), CurrentVelocity.Size(), CurrentMaxSpeed);
        */
    }

    // --- Final Speed Logging ---
    /*
    UE_LOG(LogSolaraqMovement, Log, TEXT("  BaseMaxSpeed: %.2f, Final SpeedScaleMultiplier: %.4f, EffectiveMaxSpeed: %.2f, CurrentSpeed: %.2f"),
        BaseMaxSpeed, SpeedScaleMultiplier, CurrentMaxSpeed, CurrentVelocity.Size());
    UE_LOG(LogSolaraqMovement, Log, TEXT("SPEED CALC END ---"));
    */
}

void ASolaraqShipBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    UE_LOG(LogSolaraqGeneral, Verbose, TEXT("ASolaraqShipBase %s SetupPlayerInputComponent called."), *GetName());
}

FGenericTeamId ASolaraqShipBase::GetGenericTeamId() const
{
    return TeamId;
}

ETeamAttitude::Type ASolaraqShipBase::GetTeamAttitudeTowards(const AActor& Other) const
{
    if (const APawn* OtherPawn = Cast<const APawn>(&Other))
    {
        const IGenericTeamAgentInterface* OtherTeamAgentPawn = Cast<const IGenericTeamAgentInterface>(&Other);
        if(OtherTeamAgentPawn)
        {
            if (OtherTeamAgentPawn->GetGenericTeamId() == GetGenericTeamId()) return ETeamAttitude::Friendly;
            if (OtherTeamAgentPawn->GetGenericTeamId().GetId() != FGenericTeamId::NoTeam.GetId()) return ETeamAttitude::Hostile;
        }

        if (const IGenericTeamAgentInterface* OtherTeamAgentController = Cast<const IGenericTeamAgentInterface>(OtherPawn->GetController()))
        {
            if (OtherTeamAgentController->GetGenericTeamId() == GetGenericTeamId()) return ETeamAttitude::Friendly;
            if (OtherTeamAgentController->GetGenericTeamId().GetId() != FGenericTeamId::NoTeam.GetId()) return ETeamAttitude::Hostile;
        }
        
        // if (Cast<AAIController>(OtherPawn->GetController())) return ETeamAttitude::Hostile; // Removed this as it makes all AI hostile by default
    }
    return ETeamAttitude::Neutral;
}

void ASolaraqShipBase::RequestInteraction()
{
    if (IsShipDocked() && ActiveDockingPad)
    {
        FName TargetLevelName = TEXT("CharacterTestLevel"); // Default character level name
        if (!CharacterLevelOverrideName.IsNone())
        {
            TargetLevelName = CharacterLevelOverrideName;
        }

        FName PadID = ActiveDockingPad->GetFName();
        if (ActiveDockingPad->DockingPadUniqueID != NAME_None)
        {
            PadID = ActiveDockingPad->DockingPadUniqueID;
        }
        
        // Call the server RPC to request the transition
        Server_RequestTransitionToCharacterLevel(TargetLevelName, PadID);
    }
    else
    {
        UE_LOG(LogSolaraqTransition, Warning, TEXT("Ship %s: Interaction requested, but not properly docked."), *GetName());
    }
}

void ASolaraqShipBase::Server_RequestTransitionToCharacterLevel_Implementation(FName TargetLevel, FName DockingPadID)
{
    if (!HasAuthority()) // Should always be true here, but good practice
    {
        return;
    }

    AController* MyController = GetController();
    if (!MyController)
    {
        UE_LOG(LogSolaraqTransition, Error, TEXT("Ship %s: Server_RequestTransitionToCharacterLevel - GetController() returned NULL!"), *GetName());
        return;
    }

    // Assuming your PlayerControllers derive from a common base that handles the travel initiation
    ASolaraqBasePlayerController* PC = Cast<ASolaraqBasePlayerController>(MyController);
    if (PC)
    {
        UE_LOG(LogSolaraqTransition, Log, TEXT("Ship %s: Telling its PlayerController %s to initiate character transition to Level: %s, PadID: %s"), 
            *GetName(), *PC->GetName(), *TargetLevel.ToString(), *DockingPadID.ToString());

        // The PlayerController will handle GameInstance prep and then call ClientTravel on itself (server-side instance).
        PC->Server_InitiateSeamlessTravelToLevel(TargetLevel, true, DockingPadID);
    }
    else
    {
        UE_LOG(LogSolaraqTransition, Error, TEXT("Ship %s: Server_RequestTransitionToCharacterLevel - Failed to cast Controller %s to ASolaraqBasePlayerController."), *GetName(), *MyController->GetName());
    }
}

void ASolaraqShipBase::OnRep_TurnInputForRoll()
{
    // Client tick will pick this up for visual interpolation
}

void ASolaraqShipBase::OnRep_IronCount()
{
    UE_LOG(LogSolaraqSystem, VeryVerbose, TEXT("CLIENT %s OnRep_IronCount: %d"), *GetName(), CurrentIronCount);
    OnInventoryUpdated();
}

void ASolaraqShipBase::OnRep_CrystalCount()
{
    UE_LOG(LogSolaraqSystem, VeryVerbose, TEXT("CLIENT %s OnRep_CrystalCount: %d"), *GetName(), CurrentCrystalCount);
    OnInventoryUpdated();
}

void ASolaraqShipBase::OnRep_StandardAmmo()
{
    UE_LOG(LogSolaraqSystem, VeryVerbose, TEXT("CLIENT %s OnRep_StandardAmmo: %d"), *GetName(), CurrentStandardAmmo);
    OnInventoryUpdated();
}

void ASolaraqShipBase::OnRep_CurrentShieldEnergy()
{
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (CLIENT): OnRep_CurrentShieldEnergy CALLED. New Value: %.1f. Calling UpdateShieldVisuals."),
        *GetNameSafe(this), CurrentShieldEnergy);
    UpdateShieldVisuals(); 
}

void ASolaraqShipBase::OnRep_IsShieldActive()
{
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (CLIENT): OnRep_IsShieldActive CALLED. New Value: %d. Calling UpdateShieldVisuals."),
       *GetNameSafe(this), bIsShieldActive);
    UpdateShieldVisuals();
}

void ASolaraqShipBase::OnRep_CurrentShieldStrength()
{
    
}

void ASolaraqShipBase::Multicast_PlayShieldActivationEffects_Implementation()
{
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (%s): Multicast_PlayShieldActivationEffects_Implementation CALLED. Calling UpdateShieldVisuals."),
       *GetNameSafe(this), (GetNetMode() == NM_Client ? TEXT("CLIENT") : TEXT("SERVER/OTHER")));
    UpdateShieldVisuals(); 
}

void ASolaraqShipBase::Multicast_PlayShieldDeactivationEffects_Implementation(bool bWasBrokenOrEmptied)
{
    NET_LOG(LogSolaraqCombat, Verbose, TEXT("Multicast_PlayShieldDeactivationEffects executed. WasBroken: %d"), bWasBrokenOrEmptied);
    UpdateShieldVisuals(); 
}

void ASolaraqShipBase::Multicast_PlayShieldImpactEffects_Implementation(FVector ImpactLocation, float DamageAbsorbed)
{
    NET_LOG(LogSolaraqCombat, Verbose, TEXT("Multicast_PlayShieldImpactEffects executed at %s for %.1f damage."), *ImpactLocation.ToString(), DamageAbsorbed);
}

void ASolaraqShipBase::SetTurnInputForRoll(float TurnValue)
{
    float ClampedValue = FMath::Clamp(TurnValue, -1.0f, 1.0f);
    if (HasAuthority())
    {
        if(CurrentTurnInputForRoll != ClampedValue)
        {
            CurrentTurnInputForRoll = ClampedValue;
        }
    }
    // Client uses its own immediate input for visual roll if locally controlled,
    // or relies on replicated CurrentTurnInputForRoll. Tick handles interpolation.
}

void ASolaraqShipBase::PerformFireWeapon()
{
    if (!HasAuthority() || IsDead() || IsShipDockedOrDocking() || bIsUnderScalingEffect_Server) return;

    
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    if (CurrentTime < LastFireTime + FireRate) return;

    if (!ProjectileClass) { UE_LOG(LogSolaraqCombat, Error, TEXT("%s PerformFireWeapon: ProjectileClass is NULL!"), *GetName()); return; }
    if (!MuzzlePoint) { UE_LOG(LogSolaraqCombat, Error, TEXT("%s PerformFireWeapon: MuzzlePoint is NULL!"), *GetName()); return; }
    UWorld* const World = GetWorld();
    if (!World) { UE_LOG(LogSolaraqCombat, Error, TEXT("%s PerformFireWeapon: GetWorld() is NULL!"), *GetName()); return; }

    const FVector MuzzleLocation = MuzzlePoint->GetComponentLocation();
    const FRotator MuzzleRotation = MuzzlePoint->GetComponentRotation();
    const FVector ShipVelocity = CollisionAndPhysicsRoot ? CollisionAndPhysicsRoot->GetPhysicsLinearVelocity() : FVector::ZeroVector;
    const FVector MuzzleVelocity = MuzzleRotation.Vector() * ProjectileMuzzleSpeed;
    const FVector FinalVelocity = ShipVelocity + MuzzleVelocity;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AActor* SpawnedProjectileActor = World->SpawnActor<ASolaraqProjectile>(ProjectileClass, MuzzleLocation, MuzzleRotation, SpawnParams);

    if (ASolaraqProjectile* SpawnedProjectile = Cast<ASolaraqProjectile>(SpawnedProjectileActor))
    {
        UProjectileMovementComponent* ProjMoveComp = SpawnedProjectile->GetProjectileMovement();
        if (ProjMoveComp)
        {
            ProjMoveComp->Velocity = FinalVelocity;
            ProjMoveComp->UpdateComponentVelocity(); // Ensure change takes immediate effect
            NET_LOG(LogSolaraqProjectile, Log, TEXT("Spawned %s, Set Velocity to %s"), *SpawnedProjectile->GetName(), *FinalVelocity.ToString());
            LastFireTime = CurrentTime;
        }
        else
        {
            NET_LOG(LogSolaraqProjectile, Warning, TEXT("Spawned %s but it has NO ProjectileMovementComponent!"), *SpawnedProjectile->GetName());
        }
    }
    else
    {
         NET_LOG(LogSolaraqProjectile, Error, TEXT("World->SpawnActor failed for %s!"), *ProjectileClass->GetName());
    }
}

void ASolaraqShipBase::Multicast_PlayDestructionEffects_Implementation()
{
    if (ShipMeshComponent)
    {
        ShipMeshComponent->SetVisibility(false, true);
        ShipMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    NET_LOG(LogSolaraqCombat, Verbose, TEXT("Multicast_PlayDestructionEffects executed on %s"), *GetName());
}

void ASolaraqShipBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASolaraqShipBase, CurrentEnergy);
    DOREPLIFETIME(ASolaraqShipBase, bIsAttemptingBoostInput);
    DOREPLIFETIME(ASolaraqShipBase, bIsBoosting);
    DOREPLIFETIME_CONDITION(ASolaraqShipBase, MaxEnergy, COND_InitialOnly);
    DOREPLIFETIME(ASolaraqShipBase, CurrentHealth);
    DOREPLIFETIME(ASolaraqShipBase, bIsDead);
    DOREPLIFETIME(ASolaraqShipBase, CurrentTurnInputForRoll);
    DOREPLIFETIME(ASolaraqShipBase, CurrentIronCount);
    DOREPLIFETIME(ASolaraqShipBase, CurrentCrystalCount);
    DOREPLIFETIME(ASolaraqShipBase, CurrentStandardAmmo);
    // --- Docking Replication ---
    DOREPLIFETIME(ASolaraqShipBase, CurrentDockingStatus);
    DOREPLIFETIME(ASolaraqShipBase, ActiveDockingPad);
    // --- Shield Replication ---
    DOREPLIFETIME_CONDITION(ASolaraqShipBase, MaxShieldEnergy, COND_InitialOnly);
    DOREPLIFETIME(ASolaraqShipBase, CurrentShieldEnergy);
    DOREPLIFETIME(ASolaraqShipBase, bIsShieldActive);
    DOREPLIFETIME_CONDITION(ASolaraqShipBase, MaxShieldStrength, COND_InitialOnly);
    DOREPLIFETIME(ASolaraqShipBase, CurrentShieldStrength);
}

void ASolaraqShipBase::OnRep_CurrentEnergy()
{
    UE_LOG(LogSolaraqMovement, VeryVerbose, TEXT("CLIENT OnRep_CurrentEnergy: %.2f"), CurrentEnergy);
}

void ASolaraqShipBase::OnRep_IsBoosting()
{
    UE_LOG(LogSolaraqMovement, VeryVerbose, TEXT("CLIENT OnRep_IsBoosting: %d"), bIsBoosting);
}

void ASolaraqShipBase::Server_RequestFire_Implementation()
{
    PerformFireWeapon();
}

void ASolaraqShipBase::OnRep_CurrentHealth()
{
    NET_LOG(LogSolaraqCombat, VeryVerbose, TEXT("CLIENT OnRep_CurrentHealth: %.1f/%.1f"), CurrentHealth, MaxHealth);
}

void ASolaraqShipBase::OnRep_IsDead()
{
    NET_LOG(LogSolaraqCombat, Log, TEXT("CLIENT OnRep_IsDead: %d"), bIsDead);
    if (bIsDead)
    {
        if (ShipMeshComponent && ShipMeshComponent->IsVisible())
        {
            ShipMeshComponent->SetVisibility(false, true);
        }
        SetActorEnableCollision(ECollisionEnabled::NoCollision);
        if (CollisionAndPhysicsRoot) CollisionAndPhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void ASolaraqShipBase::SetEffectiveScaleFactor_Server(float NewScaleFactor)
{
    if (HasAuthority())
    {
        // Clamp the incoming scale factor to be safe, though CelestialBodyBase should already do this.
        CurrentEffectiveScaleFactor_Server = FMath::Clamp(NewScaleFactor, 0.01f, 1.0f); // Assuming min scale is 0.01
    }
}

void ASolaraqShipBase::HandleDestruction()
{
    if (!HasAuthority() || bIsDead) return;

    NET_LOG(LogSolaraqCombat, Log, TEXT("Ship Destroyed!"));
    bIsDead = true;
    Multicast_PlayDestructionEffects();

    if (CollisionAndPhysicsRoot)
    {
        CollisionAndPhysicsRoot->SetSimulatePhysics(false);
        CollisionAndPhysicsRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);
        CollisionAndPhysicsRoot->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }
    SetActorTickEnabled(false);
    SetActorEnableCollision(ECollisionEnabled::NoCollision);
    if (CollisionAndPhysicsRoot) CollisionAndPhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (ShipMeshComponent) ShipMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    AController* CurrentController = GetController();
    if (CurrentController) CurrentController->UnPossess();
    SetLifeSpan(5.0f);
}

// --- Docking Implementation ---

bool ASolaraqShipBase::IsShipDockedOrDocking() const
{
    return bIsLerpingToDockPosition || // Check lerping state first
           CurrentDockingStatus == EDockingStatus::Docked ||
           CurrentDockingStatus == EDockingStatus::Docking || // "Docking" is now our lerp phase
           CurrentDockingStatus == EDockingStatus::AttemptingDock; // AttemptingDock could be a brief pre-lerp state if you add one
}

bool ASolaraqShipBase::IsDockedToPad(const UDockingPadComponent* Pad) const
{
    return IsShipDocked() && (ActiveDockingPad == Pad);
}

void ASolaraqShipBase::Server_RequestDockWithPad_Implementation(UDockingPadComponent* PadToDockWith)
{
    // Cooldown Check: Can we even attempt to dock yet?
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    if (LastUndockTime > 0.f && CurrentTime < LastUndockTime + DockingCooldownDuration)
    {
        NET_LOG(LogSolaraqSystem, Warning, TEXT("Server_RequestDockWithPad failed. Still in docking cooldown. Time remaining: %.1fs"), (LastUndockTime + DockingCooldownDuration) - CurrentTime);
        return;
    }
    
    if (!PadToDockWith || !PadToDockWith->GetOwner())
    {
        NET_LOG(LogSolaraqSystem, Warning, TEXT("Server_RequestDockWithPad failed. Invalid pad."));
        return;
    }
    if (IsShipDockedOrDocking())
    {
        NET_LOG(LogSolaraqSystem, Warning, TEXT("Server_RequestDockWithPad failed. Already docked or docking to %s."), *GetNameSafe(ActiveDockingPad));
        return;
    }
    if (!PadToDockWith->IsPadFree_Server())
    {
        NET_LOG(LogSolaraqSystem, Warning, TEXT("Server_RequestDockWithPad failed. Pad %s is not free."), *PadToDockWith->GetName());
        return;
    }

    NET_LOG(LogSolaraqSystem, Log, TEXT("Server_RequestDockWithPad with %s. Current Status: %s"), *PadToDockWith->GetName(), *UEnum::GetValueAsString(CurrentDockingStatus));

    CurrentDockingStatus = EDockingStatus::Docking; 
    ActiveDockingPad = PadToDockWith;
    ActiveDockingPad->SetOccupyingShip_Server(this);
    CurrentDockingStartTime = CurrentTime;
    

    // --- INITIATE LERP INSTEAD OF INSTANT ATTACH & SNAP ---
    // Physics will be disabled, but attachment happens AFTER lerp or during.
    // For simplicity, let's disable physics now and attach, then lerp relative transform.
    if (CollisionAndPhysicsRoot)
    {
        CollisionAndPhysicsRoot->SetSimulatePhysics(false);
        CollisionAndPhysicsRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);
        CollisionAndPhysicsRoot->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }

    // Attach the ship immediately but keep its world transform. The lerp will adjust its relative transform.
    AActor* PadOwner = ActiveDockingPad->GetOwner();
    if (PadOwner && ActiveDockingPad->GetAttachPoint())
    {
        LerpAttachTargetComponent = ActiveDockingPad->GetAttachPoint(); // Store for Tick
        
        // --- Store current world rotation BEFORE attachment for accurate calculation ---
        const FRotator ShipWorldRotationAtDockStart = GetActorRotation();
        
        FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
        GetRootComponent()->AttachToComponent(LerpAttachTargetComponent, AttachRules);

        const FTransform PadWorldTransform = LerpAttachTargetComponent->GetComponentTransform();
        

        // --- SAVE DOCKED ROTATION TO GAME INSTANCE ---
        USolaraqGameInstance* GI = GetGameInstance<USolaraqGameInstance>();
        if (GI)
        {
            ActualDockingTargetRelativeRotation = (PadWorldTransform.GetRotation().Inverse() * FQuat(ShipWorldRotationAtDockStart)).Rotator();
            GI->ShipDockedRelativeRotation = ActualDockingTargetRelativeRotation;
            UE_LOG(LogSolaraqSystem, Log, TEXT("Ship %s: Saved initial docked relative rotation %s to GameInstance. This will be the lerp target."),
                *GetName(), *ActualDockingTargetRelativeRotation.ToString());
        }
        
        NET_LOG(LogSolaraqSystem, Verbose, TEXT("Ship %s: WorldRotAtDockStart: %s, PadWorldRot: %s, TargetRelRot: %s"),
            *GetName(), *ShipWorldRotationAtDockStart.ToString(), *PadWorldTransform.GetRotation().Rotator().ToString(), *ActualDockingTargetRelativeRotation.ToString());
        
        bIsLerpingToDockPosition = true; // Start the lerp
        NET_LOG(LogSolaraqSystem, Log, TEXT("Ship %s attached to %s. Initiating lerp to dock position."), *GetName(), *LerpAttachTargetComponent->GetName());
        
        // Reset visual scale if needed
        Client_ResetVisualScale();
        ApplyVisualScale(1.0f);
    }
    else
    {
        NET_LOG(LogSolaraqSystem, Error, TEXT("Ship %s: Failed to initiate docking lerp. Invalid pad owner or attach point."), *GetName());
        // Rollback docking attempt
        if(ActiveDockingPad) ActiveDockingPad->ClearOccupyingShip_Server();
        ActiveDockingPad = nullptr;
        CurrentDockingStatus = EDockingStatus::None;
        CurrentDockingStartTime = -1.0f; 
        if (CollisionAndPhysicsRoot) CollisionAndPhysicsRoot->SetSimulatePhysics(true); // Re-enable physics
    }
    // --- END LERP INITIATION ---


    OnRep_DockingStateChanged(); // Call on server for consistency
}

void ASolaraqShipBase::Server_RequestUndock_Implementation()
{
    // --- SERVER ONLY ---
    if (CurrentDockingStatus != EDockingStatus::Docked && CurrentDockingStatus != EDockingStatus::Docking) // Allow undock if lerping too
    {
        NET_LOG(LogSolaraqSystem, Warning, TEXT("Server_RequestUndock failed. Not currently docked or docking. Status: %s"),
            *UEnum::GetValueAsString(CurrentDockingStatus));
        return;
    }
    // ... (rest of undock logic as before) ...

    // Crucially, stop any active lerp
    if (bIsLerpingToDockPosition)
    {
        bIsLerpingToDockPosition = false;
        LerpAttachTargetComponent = nullptr;
        NET_LOG(LogSolaraqSystem, Log, TEXT("Undock requested during docking lerp. Lerp cancelled."));
        CurrentDockingStartTime = -1.0f;
    }

    // ... (the rest of the Server_RequestUndock_Implementation)
    // (e.g., PerformUndockingDetachmentFromPad, ClearOccupyingShip_Server, setting status to None)
    if (!ActiveDockingPad && CurrentDockingStatus != EDockingStatus::None) // If pad was cleared by lerp cancellation but status not reset
    {
         NET_LOG(LogSolaraqSystem, Warning, TEXT("Server_RequestUndock: ActiveDockingPad became null during processing, likely due to lerp cancellation. Forcing undock."));
    }
    else if (!ActiveDockingPad && CurrentDockingStatus == EDockingStatus::None)
    {
        // Already effectively undocked by lerp cancellation path
        NET_LOG(LogSolaraqSystem, Log, TEXT("Server_RequestUndock: Ship already in 'None' state post-lerp cancellation."));
        return;
    }
    else if (!ActiveDockingPad) // Should not happen if states are managed properly
    {
        NET_LOG(LogSolaraqSystem, Error, TEXT("Server_RequestUndock failed. Docked but ActiveDockingPad is null!"));
        CurrentDockingStatus = EDockingStatus::None;
        if (CollisionAndPhysicsRoot && !CollisionAndPhysicsRoot->IsSimulatingPhysics()) CollisionAndPhysicsRoot->SetSimulatePhysics(true);
        OnRep_DockingStateChanged();
        return;
    }


    NET_LOG(LogSolaraqSystem, Log, TEXT("Server_RequestUndock from %s. Current Status: %s"), *GetNameSafe(ActiveDockingPad), *UEnum::GetValueAsString(CurrentDockingStatus));
    
    EDockingStatus PreviousStatus = CurrentDockingStatus;
    CurrentDockingStatus = EDockingStatus::Undocking; 
    PerformUndockingDetachmentFromPad();
    
    if(ActiveDockingPad) ActiveDockingPad->ClearOccupyingShip_Server();
    ActiveDockingPad = nullptr;
    CurrentDockingStatus = EDockingStatus::None;
    CurrentDockingStartTime = -1.0f;

    LastUndockTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    NET_LOG(LogSolaraqSystem, Log, TEXT("Ship undocked. Docking cooldown initiated for %.1fs."), DockingCooldownDuration);
    
    // Only call OnRep if the status actually changed from the start of this function
    // or if it was 'Docking' and now 'None' (lerp interruption)
    if (PreviousStatus != EDockingStatus::None || CurrentDockingStatus != EDockingStatus::None)
    {
        OnRep_DockingStateChanged(); 
    }
}

void ASolaraqShipBase::PerformUndockingDetachmentFromPad()
{
    // --- SERVER ONLY ---
    if (!HasAuthority() || !CollisionAndPhysicsRoot) return;

    NET_LOG(LogSolaraqSystem, Log, TEXT("Performing undocking detachment."));

    // Ensure lerp is stopped if undocking happens while lerping (shouldn't typically occur if states are managed well)
    if (bIsLerpingToDockPosition)
    {
        bIsLerpingToDockPosition = false;
        NET_LOG(LogSolaraqSystem, Warning, TEXT("Undocking while lerp was active. Stopping lerp."));
    }
    LerpAttachTargetComponent = nullptr;


    // 1. Detach
    FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld, true);
    GetRootComponent()->DetachFromComponent(DetachRules);

    // 2. Re-enable Physics (AFTER detaching)
    CollisionAndPhysicsRoot->SetSimulatePhysics(true);
    if (FBodyInstance* BodyInst = CollisionAndPhysicsRoot->GetBodyInstance())
    {
        BodyInst->LinearDamping = Dampening;
        BodyInst->AngularDamping = 0.8f;
    }

    Internal_EnableSystemsAfterUndocking();
}

void ASolaraqShipBase::Internal_DisableSystemsForDocking()
{
    NET_LOG(LogSolaraqSystem, Verbose, TEXT("Disabling systems for docking."));
    if (bIsBoosting || bIsAttemptingBoostInput)
    {
        Server_SetAttemptingBoost(false); // This will turn off bIsBoosting in Tick
    }
    // Additional systems to disable (e.g., weapon activation) would go here
}

void ASolaraqShipBase::SetUnderScalingEffect_Server(bool bIsBeingScaled)
{
    // This function should only be called on the server.
    if (HasAuthority())
    {
        bIsUnderScalingEffect_Server = bIsBeingScaled;
    }
}

void ASolaraqShipBase::Internal_EnableSystemsAfterUndocking()
{
    NET_LOG(LogSolaraqSystem, Verbose, TEXT("Enabling systems after undocking."));
    // Re-enable systems
}

void ASolaraqShipBase::Server_ActivateShield()
{
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Server_ActivateShield CALLED."), *GetNameSafe(this));
    if (!HasAuthority() || bIsShieldActive)
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Server_ActivateShield returning. HasAuthority: %d, bIsShieldActive: %d."),
            *GetNameSafe(this), HasAuthority(), bIsShieldActive);
        return;
    }

    // Check if there's enough DURATION energy
    if (CurrentShieldEnergy < MinEnergyToActivateShield)
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Cannot activate shield: Not enough DURATION energy (%.1f / %.1f required). CurrentStrength: %.1f"),
            *GetNameSafe(this), CurrentShieldEnergy, MinEnergyToActivateShield, CurrentShieldStrength);
        return;
    }

    bIsShieldActive = true;
    CurrentShieldStrength = MaxShieldStrength; // Set shield HP to full
    LastShieldDeactivationTime = -1.0f;

    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Shield HP (Strength) set to full: %.1f. DURATION Energy: %.1f"),
        *GetNameSafe(this), CurrentShieldStrength, CurrentShieldEnergy);

    ClearAllShieldTimers();
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Setting TimerHandle_ShieldDrain for DURATION. Interval: %.2f"), *GetNameSafe(this), ShieldTimerUpdateInterval);
    GetWorldTimerManager().SetTimer(TimerHandle_ShieldDrain, this, &ASolaraqShipBase::Server_ProcessShieldDrain, ShieldTimerUpdateInterval, true);

    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Shield ACTIVATED. DURATION: %.1f. STRENGTH: %.1f. Calling Multicast & OnRep."),
        *GetNameSafe(this), CurrentShieldEnergy, CurrentShieldStrength);
    Multicast_PlayShieldActivationEffects();
    OnRep_IsShieldActive();
}

void ASolaraqShipBase::Server_DeactivateShield(bool bForcedByEmptyOrBreak, bool bSkipCooldown)
{
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Server_DeactivateShield CALLED. bForcedByEmptyOrBreak: %d, bSkipCooldown: %d"), // Corrected parameter name in log
        *GetNameSafe(this), bForcedByEmptyOrBreak, bSkipCooldown);
    if (!HasAuthority() || !bIsShieldActive)
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Server_DeactivateShield returning. HasAuthority: %d, bIsShieldActive: %d."),
            *GetNameSafe(this), HasAuthority(), bIsShieldActive);
        return;
    }

    bIsShieldActive = false;
    float PreviousStrength = CurrentShieldStrength;
    CurrentShieldStrength = 0.0f; // Shield HP is 0 when inactive
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Shield HP (Strength) set to 0. Was: %.1f. Current DURATION Energy: %.1f"),
        *GetNameSafe(this), PreviousStrength, CurrentShieldEnergy);


    if (!bSkipCooldown) {
        LastShieldDeactivationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): LastShieldDeactivationTime set to: %.2f (Cooldown NOT skipped)."), *GetNameSafe(this), LastShieldDeactivationTime);
    } else {
        LastShieldDeactivationTime = -1.0f;
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): LastShieldDeactivationTime set to -1.0 (Cooldown SKIPPED)."), *GetNameSafe(this));
    }

    ClearAllShieldTimers(); // Stop duration drain
    if (!bSkipCooldown) // Start DURATION energy regen process
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Setting TimerHandle_ShieldRegenDelayCheck for DURATION energy. Delay: %.2f."), *GetNameSafe(this), ShieldRegenDelay);
        GetWorldTimerManager().SetTimer(TimerHandle_ShieldRegenDelayCheck, this, &ASolaraqShipBase::Server_TryStartShieldRegenTimer, ShieldRegenDelay, false);
    }
    else
    {
         UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): SKIPPING ShieldRegenDelayCheck timer start (Cooldown SKIPPED)."), *GetNameSafe(this));
    }

    FString Reason = bForcedByEmptyOrBreak ? TEXT("forced (depleted duration/strength)") : TEXT("player toggle"); // Using bForcedByEmptyOrBreak
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Shield DEACTIVATED (%s). DURATION: %.1f. STRENGTH now 0. Calling Multicast & OnRep."),
        *GetNameSafe(this), *Reason, CurrentShieldEnergy);
    Multicast_PlayShieldDeactivationEffects(bForcedByEmptyOrBreak); // Pass the correct parameter
    OnRep_IsShieldActive();
}

void ASolaraqShipBase::UpdateShieldVisuals()
{
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (%s): UpdateShieldVisuals CALLED. bIsShieldActive: %d. ShieldMeshComponent valid: %d"),
        *GetNameSafe(this), (GetNetMode() == NM_Client ? TEXT("CLIENT") : TEXT("SERVER/OTHER")), bIsShieldActive, ShieldMeshComponent != nullptr);
    if (ShieldMeshComponent)
    {
        ShieldMeshComponent->SetVisibility(bIsShieldActive);
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (%s): ShieldMesh visibility set to: %s"),
            *GetNameSafe(this), (GetNetMode() == NM_Client ? TEXT("CLIENT") : TEXT("SERVER/OTHER")), bIsShieldActive ? TEXT("Visible") : TEXT("Hidden"));
    }
}

void ASolaraqShipBase::Server_ProcessShieldDrain()
{
    // This can be very spammy, consider reducing verbosity or frequency if not actively debugging drain
    // UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Server_ProcessShieldDrain CALLED."), *GetNameSafe(this));
    if (!HasAuthority() || !bIsShieldActive || IsDead())
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Server_ProcessShieldDrain stopping/returning. HasAuth: %d, IsActive: %d, IsDead: %d. Clearing timers."),
            *GetNameSafe(this), HasAuthority(), bIsShieldActive, IsDead());
        ClearAllShieldTimers();
        return;
    }

    float EnergyBeforeDrain = CurrentShieldEnergy;
    CurrentShieldEnergy -= ShieldEnergyDrainRate * ShieldTimerUpdateInterval;
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Shield DURATION Draining. Before: %.2f, After: %.2f. Current STRENGTH: %.1f (unaffected by DURATION drain)."), // Ensure uncommented
        *GetNameSafe(this), EnergyBeforeDrain, CurrentShieldEnergy, CurrentShieldStrength);

    if (CurrentShieldEnergy <= 0.0f)
    {
        CurrentShieldEnergy = 0.0f;
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Shield DURATION energy depleted. Calling Server_DeactivateShield(true, false) (Forced=true, SkipCooldown=false)."), *GetNameSafe(this));
        Server_DeactivateShield(true, false); // Duration ran out
    }
}

void ASolaraqShipBase::Server_TryStartShieldRegenTimer()
{
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Server_TryStartShieldRegenTimer CALLED."), *GetNameSafe(this));
    if (!HasAuthority() || bIsShieldActive || IsDead())
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Server_TryStartShieldRegenTimer stopping/returning. HasAuth: %d, IsActive: %d, IsDead: %d. Clearing RegenDelayCheck timer."),
            *GetNameSafe(this), HasAuthority(), bIsShieldActive, IsDead());
        GetWorldTimerManager().ClearTimer(TimerHandle_ShieldRegenDelayCheck); // Clear this delay timer if conditions aren't met
        return;
    }
    
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Clearing TimerHandle_ShieldRegenDelayCheck."), *GetNameSafe(this));
    GetWorldTimerManager().ClearTimer(TimerHandle_ShieldRegenDelayCheck);

    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): CurrentTime: %.2f. LastShieldDeactivationTime: %.2f, ShieldActivationCooldown: %.2f."),
        *GetNameSafe(this), CurrentTime, LastShieldDeactivationTime, ShieldActivationCooldown);
        
    if (LastShieldDeactivationTime > 0.f && CurrentTime < LastShieldDeactivationTime + ShieldActivationCooldown)
    {
        float RemainingCooldown = (LastShieldDeactivationTime + ShieldActivationCooldown) - CurrentTime;
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Shield regen delay passed, but still in activation cooldown. Rescheduling regen start in %.2fs."),
            *GetNameSafe(this), RemainingCooldown);
        GetWorldTimerManager().SetTimer(TimerHandle_ShieldRegenDelayCheck, this, &ASolaraqShipBase::Server_TryStartShieldRegenTimer, RemainingCooldown, false);
        return;
    }

    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): CurrentShieldEnergy: %.1f, MaxShieldEnergy: %.1f."),
        *GetNameSafe(this), CurrentShieldEnergy, MaxShieldEnergy);
    if (CurrentShieldEnergy < MaxShieldEnergy)
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Starting TimerHandle_ShieldRegen. Interval: %.2f"), *GetNameSafe(this), ShieldTimerUpdateInterval);
        GetWorldTimerManager().SetTimer(TimerHandle_ShieldRegen, this, &ASolaraqShipBase::Server_ProcessShieldRegen, ShieldTimerUpdateInterval, true);
    }
    else
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Shield already at max energy (%.1f). Not starting regen timer."), *GetNameSafe(this), CurrentShieldEnergy);
    }
}

void ASolaraqShipBase::Server_ProcessShieldRegen()
{
    // This can be very spammy, consider reducing verbosity or frequency if not actively debugging regen
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Server_ProcessShieldRegen CALLED."), *GetNameSafe(this));
    if (!HasAuthority() || bIsShieldActive || IsDead())
    {
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Server_ProcessShieldRegen stopping/returning. HasAuth: %d, IsActive: %d, IsDead: %d. Clearing timers."),
            *GetNameSafe(this), HasAuthority(), bIsShieldActive, IsDead());
        ClearAllShieldTimers();
        return;
    }

    float EnergyBeforeRegen = CurrentShieldEnergy;
    CurrentShieldEnergy = FMath::Min(CurrentShieldEnergy + ShieldEnergyRegenRate * ShieldTimerUpdateInterval, MaxShieldEnergy);
    // UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Shield Regenerating. Before: %.2f, After: %.2f (RegenRate: %.2f, Interval: %.2f)"),
    //     *GetNameSafe(this), EnergyBeforeRegen, CurrentShieldEnergy, ShieldEnergyRegenRate, ShieldTimerUpdateInterval);

    if (CurrentShieldEnergy >= MaxShieldEnergy)
    {
        CurrentShieldEnergy = MaxShieldEnergy;
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (SERVER): Shield fully regenerated to %.1f. Clearing TimerHandle_ShieldRegen."), *GetNameSafe(this), CurrentShieldEnergy);
        GetWorldTimerManager().ClearTimer(TimerHandle_ShieldRegen);
    }
}

void ASolaraqShipBase::ClearAllShieldTimers()
{
    UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (%s): ClearAllShieldTimers CALLED."), *GetNameSafe(this), (HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT/OTHER")));
    if (GetWorldTimerManager().IsTimerActive(TimerHandle_ShieldDrain))
    {
        GetWorldTimerManager().ClearTimer(TimerHandle_ShieldDrain);
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (%s): Cleared TimerHandle_ShieldDrain."), *GetNameSafe(this), (HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT/OTHER")));
    }
    if (GetWorldTimerManager().IsTimerActive(TimerHandle_ShieldRegenDelayCheck))
    {
        GetWorldTimerManager().ClearTimer(TimerHandle_ShieldRegenDelayCheck);
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (%s): Cleared TimerHandle_ShieldRegenDelayCheck."), *GetNameSafe(this), (HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT/OTHER")));
    }
    if (GetWorldTimerManager().IsTimerActive(TimerHandle_ShieldRegen))
    {
        GetWorldTimerManager().ClearTimer(TimerHandle_ShieldRegen);
        UE_LOG(LogSolaraqShield, Warning, TEXT("Ship %s (%s): Cleared TimerHandle_ShieldRegen."), *GetNameSafe(this), (HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT/OTHER")));
    }
}

bool ASolaraqShipBase::IsDockedToPadID(FName PadUniqueID) const
{
    if (IsShipDocked() && ActiveDockingPad)
    {
        // Ensure DockingPadUniqueID is set on the pad component in the editor or its construction script.
        // If DockingPadUniqueID is NAME_None, this check will fail unless PadUniqueID is also NAME_None.
        return ActiveDockingPad->DockingPadUniqueID == PadUniqueID;
    }
    return false;
}

void ASolaraqShipBase::OnRep_DockingStateChanged()
{
    NET_LOG(LogSolaraqSystem, Verbose, TEXT("CLIENT OnRep_DockingStateChanged. Status: %s, Pad: %s"),
        *UEnum::GetValueAsString(CurrentDockingStatus), *GetNameSafe(ActiveDockingPad));

    if (CurrentDockingStatus == EDockingStatus::Docked || 
        CurrentDockingStatus == EDockingStatus::Docking) // Client also treats "Docking" (lerp phase) as physics off
    {
        if (CollisionAndPhysicsRoot && CollisionAndPhysicsRoot->IsSimulatingPhysics())
        {
            CollisionAndPhysicsRoot->SetSimulatePhysics(false);
            NET_LOG(LogSolaraqSystem, Verbose, TEXT("CLIENT Disabling local physics simulation due to docking/lerping state."));
        }
        // If the ship was scaled, reset it on the client when docking starts
        if (CurrentDockingStatus == EDockingStatus::Docking || CurrentDockingStatus == EDockingStatus::Docked)
        {
            ApplyVisualScale(1.0f); 
        }
    }
    else // None, Undocking, AttemptingDock (if physics was on)
    {
        if (CollisionAndPhysicsRoot && !CollisionAndPhysicsRoot->IsSimulatingPhysics())
        {
            if (!bIsDead)
            {
                CollisionAndPhysicsRoot->SetSimulatePhysics(true);
                if (FBodyInstance* BodyInst = CollisionAndPhysicsRoot->GetBodyInstance())
                {
                    BodyInst->LinearDamping = Dampening;
                    BodyInst->AngularDamping = 0.8f;
                }
                NET_LOG(LogSolaraqSystem, Verbose, TEXT("CLIENT Re-enabling local physics simulation due to undocking/none state."));
            }
        }
    }
    // BlueprintImplementableEvent OnDockingStatusChangedBP(CurrentDockingStatus, ActiveDockingPad);
}

