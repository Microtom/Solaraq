#include "Projectiles/SolaraqHomingProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Utils/SolaraqMathLibrary.h" // Adjust path
#include "Logging/SolaraqLogChannels.h" // For logging
#include "Engine/World.h" // For GetWorld()

ASolaraqHomingProjectile::ASolaraqHomingProjectile()
{
    PrimaryActorTick.bCanEverTick = true; // Homing missiles need to tick
    PrimaryActorTick.bStartWithTickEnabled = true;

    // Sensible defaults, can be overridden in Blueprint
    HomingAccelerationMagnitude = 10000.0f; // Example value, adjust for desired agility

    // ProjectileMovement settings might be different for homing missiles
    // e.g., lower initial/max speed but able to turn.
    // These can be set in the Blueprint derived from this class.
    // Example:
    // if (ProjectileMovement)
    // {
    //     ProjectileMovement->InitialSpeed = 4000.f;
    //     ProjectileMovement->MaxSpeed = 4500.f;
    // }
}

void ASolaraqHomingProjectile::BeginPlay()
{
    Super::BeginPlay();
    // If base class tick is disabled, enable it here if this class needs it
    // PrimaryActorTick.SetTickFunctionEnable(true);
    
    // Initial log
    // UE_LOG(LogSolaraqProjectile, Log, TEXT("Homing Projectile %s spawned."), *GetName());
}


void ASolaraqHomingProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime); // Call base class tick if it does anything important

    // Homing logic should only run on the server for authority
    if (!HasAuthority())
    {
        return;
    }

    if (TargetActor.IsValid() && ProjectileMovement)
    {
        AActor* CurrentTarget = TargetActor.Get();
        // Check if target is valid and not pending kill
        if (!CurrentTarget || CurrentTarget->IsPendingKillPending() || CurrentTarget->IsUnreachable())
        {
            // UE_LOG(LogSolaraqProjectile, Log, TEXT("Homing Projectile %s: Target %s lost or invalid. Going dumb."), *GetName(), *GetNameSafe(CurrentTarget));
            TargetActor = nullptr; // Stop homing
            // Optionally, make the missile self-destruct sooner
            // if (GetLifeSpan() > 3.0f) SetLifeSpan(3.0f); 
            return;
        }

        FVector SelfLocation = GetActorLocation();
        FVector SelfVelocity = ProjectileMovement->Velocity;

        FVector TargetLocation = CurrentTarget->GetActorLocation();
        FVector TargetVelocity = CurrentTarget->GetVelocity(); // Relies on target having a component that reports velocity

        // Use the projectile's max speed for the intercept calculation.
        // This assumes the missile will try to reach MaxSpeed.
        float EffectiveProjectileSpeed = ProjectileMovement->MaxSpeed;
        if (EffectiveProjectileSpeed <= 0.f) // Safety check
        {
            // UE_LOG(LogSolaraqProjectile, Warning, TEXT("Homing Projectile %s: EffectiveProjectileSpeed is 0 or negative. Using fallback."), *GetName());
            EffectiveProjectileSpeed = 5000.f; // A default reasonable speed
        }


        FVector InterceptPoint;
        float DummyTimeToIntercept; // Declare a dummy float to satisfy the new function signature
        
        bool bCanIntercept = USolaraqMathLibrary::CalculateInterceptPoint(
            SelfLocation, SelfVelocity,
            TargetLocation, TargetVelocity,
            EffectiveProjectileSpeed, InterceptPoint, DummyTimeToIntercept
        );

        FVector AimDirection;
        if (bCanIntercept)
        {
            AimDirection = (InterceptPoint - SelfLocation).GetSafeNormal();
            // UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Homing %s to Intercept %s for %s"), *GetName(), *InterceptPoint.ToString(), *CurrentTarget->GetName());
        }
        else
        {
            // Fallback: aim directly at the target's current position
            AimDirection = (TargetLocation - SelfLocation).GetSafeNormal();
            // UE_LOG(LogSolaraqProjectile, Verbose, TEXT("Homing %s to Direct %s for %s (Intercept Failed)"), *GetName(), *TargetLocation.ToString(), *CurrentTarget->GetName());
        }

        if (AimDirection.IsNearlyZero())
        {
            // UE_LOG(LogSolaraqProjectile, Warning, TEXT("Homing Projectile %s: AimDirection is zero. Target may be at current location."), *GetName());
            return; // Avoid NaN issues if already at target
        }
        
        // Apply acceleration towards the aim direction.
        // The ProjectileMovementComponent will clamp to its MaxSpeed.
        // It will also handle bRotationFollowsVelocity.
        ProjectileMovement->AddForce(AimDirection * HomingAccelerationMagnitude);
    }
}

void ASolaraqHomingProjectile::SetupHomingTarget(AActor* NewTarget)
{
    if (HasAuthority()) // Only server should set the target authoritatively
    {
        if (NewTarget && !NewTarget->IsPendingKillPending())
        {
            TargetActor = NewTarget;
            // UE_LOG(LogSolaraqProjectile, Log, TEXT("Homing Projectile %s: Target set to %s"), *GetName(), *NewTarget->GetName());
        }
        else
        {
            // UE_LOG(LogSolaraqProjectile, Warning, TEXT("Homing Projectile %s: Attempted to set invalid target."), *GetName());
        }
    }
}

void ASolaraqHomingProjectile::OnRep_TargetActor()
{
    // Client-side reaction to target being set (e.g., special effects if needed)
    // UE_LOG(LogSolaraqProjectile, Verbose, TEXT("CLIENT Homing Projectile %s: Target %s replicated."), *GetName(), TargetActor.IsValid() ? *TargetActor->GetName() : TEXT("None"));
}

void ASolaraqHomingProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ASolaraqHomingProjectile, TargetActor, COND_InitialOnly); // Target is set on spawn and doesn't change
}