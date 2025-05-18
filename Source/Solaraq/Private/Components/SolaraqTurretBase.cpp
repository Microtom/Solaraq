#include "Components/SolaraqTurretBase.h" // Adjust path as necessary
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Projectiles/SolaraqProjectile.h" // Adjust path as necessary
#include "GameFramework/ProjectileMovementComponent.h" // To get default projectile speed
#include "Utils/SolaraqMathLibrary.h"      // Adjust path as necessary
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"
#include "GameFramework/Pawn.h"
#include "Pawns/SolaraqShipBase.h" // Adjust to your ship base class for velocity
#include "Logging/SolaraqLogChannels.h" 
#include "Engine/CollisionProfile.h" 
#include "Net/UnrealNetwork.h" // For DOREPLIFETIME

// DEFINE_LOG_CATEGORY_STATIC(LogSolaraqTurret, Log, All); // Ensure this or similar is in your LogChannels.cpp

ATurretBase::ATurretBase()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f; 

    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    RootComponent = RootSceneComponent;

    TurretYawPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretYawPivot"));
    TurretYawPivot->SetupAttachment(RootSceneComponent); 
    TurretYawPivot->SetIsReplicated(true); // Replicate this component's state

    TurretGunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretGunMesh"));
    TurretGunMesh->SetupAttachment(TurretYawPivot);
    TurretGunMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

    MuzzleLocationComponent = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
    MuzzleLocationComponent->SetupAttachment(TurretGunMesh); 

    bReplicates = true;
    // SetReplicatingMovement(true); // Replicates root component movement, YawPivot is handled separately

    ClientTargetYawPivotRelativeRotation = FRotator::ZeroRotator; // Initialize client target

    UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: Constructor finished."), *GetName());
}

void ATurretBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ATurretBase, ReplicatedYawPivotRelativeRotation, COND_SkipOwner); // Replicate to clients, not to owning client if pawn
}

void ATurretBase::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: BeginPlay START. NetMode: %d"), *GetName(), GetNetMode());

    FireCooldownRemaining = InitialFireDelay; 
    UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: InitialFireDelay set to %.2f."), *GetName(), InitialFireDelay);

    if (ProjectileClass)
    {
        UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: ProjectileClass is %s."), *GetName(), *ProjectileClass->GetName());
        ASolaraqProjectile* DefaultProjectile = ProjectileClass->GetDefaultObject<ASolaraqProjectile>();
        if (DefaultProjectile)
        {
            UProjectileMovementComponent* ProjMoveComp = DefaultProjectile->GetProjectileMovementComponent();
            if (ProjMoveComp)
            {
                ActualProjectileSpeed = ProjMoveComp->InitialSpeed; // Read from CDO
                UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: Got ActualProjectileSpeed %.2f from ProjectileClass default object's InitialSpeed."), *GetName(), ActualProjectileSpeed);
            }
            else
            {
                UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: ProjectileClass %s's default object does NOT have a valid ProjectileMovementComponent. Using fallback speed 5000."), *GetName(), *GetNameSafe(ProjectileClass));
                ActualProjectileSpeed = 5000.0f; // Fallback
            }
        }
        else
        {
            UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: Could NOT get DefaultObject from ProjectileClass %s. Using fallback speed 5000."), *GetName(), *GetNameSafe(ProjectileClass));
            ActualProjectileSpeed = 5000.0f; // Fallback
        }
    }
    else
    {
        UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: ProjectileClass is NOT set! Turret will not fire."), *GetName());
        bCanFire = false; 
        ActualProjectileSpeed = 0.0f; // No projectile, so speed is irrelevant but set to 0
    }

    if (ProjectileSpeedOverride > 0.0f)
    {
        ActualProjectileSpeed = ProjectileSpeedOverride;
        UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: Using ProjectileSpeedOverride: %.2f."), *GetName(), ActualProjectileSpeed);
    }
    
    if (ActualProjectileSpeed <= 0.0f && ProjectileClass) // Check if it's still zero despite having a projectile class
    {
         UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: ActualProjectileSpeed is STILL <= 0 (%.2f) after checks. Defaulting to 5000. Turret may not aim or fire correctly. CHECK BLUEPRINT DEFAULTS FOR PROJECTILE SPEED!"), *GetName(), ActualProjectileSpeed);
         ActualProjectileSpeed = 5000.0f; // Prevent division by zero / ensure valid speed
    }

    UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: Final ActualProjectileSpeed for aiming/firing: %.2f"), *GetName(), ActualProjectileSpeed);
    
    if (TurretYawPivot)
    {
        ClientTargetYawPivotRelativeRotation = TurretYawPivot->GetRelativeRotation(); // Initialize with current
        ReplicatedYawPivotRelativeRotation = TurretYawPivot->GetRelativeRotation(); // Init replicated value too
    }
    
    UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: BeginPlay END."), *GetName());

    // Initialize SmoothedAimWorldLocation to a point in front of the turret
    // or at its current target if one is immediately found.
    // For simplicity, let's initialize it based on the muzzle's forward direction.
    if (MuzzleLocationComponent)
    {
        SmoothedAimWorldLocation = GetMuzzleLocation() + GetMuzzleRotation().Vector() * TargetingRange * 0.5f; // Aim somewhat forward initially
    }
    else
    {
        SmoothedAimWorldLocation = GetActorLocation() + GetActorForwardVector() * TargetingRange * 0.5f;
    }

    if (TurretYawPivot)
    {
        ClientTargetYawPivotRelativeRotation = TurretYawPivot->GetRelativeRotation(); 
        ReplicatedYawPivotRelativeRotation = TurretYawPivot->GetRelativeRotation(); 
    }
    
    UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: BeginPlay END. Initial SmoothedAimWorldLocation: %s"), *GetName(), *SmoothedAimWorldLocation.ToString());
}

void ATurretBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority()) // Server handles main logic
    {
        if (TurretYawPivot)
        {
             UE_LOG(LogSolaraqTurret, Warning, TEXT("SERVER Turret %s: Tick: Current Actual Relative Yaw: %.2f. ReplicatedYaw: %.2f"), 
                *GetName(), 
                TurretYawPivot->GetRelativeRotation().Yaw,
                ReplicatedYawPivotRelativeRotation.Yaw);
        }

        if (FireCooldownRemaining > 0.0f)
        {
            FireCooldownRemaining -= DeltaTime;
        }

        if (!ProjectileClass) 
        {
            return;
        }

        // Target acquisition and validation
        if (!CurrentTarget.IsValid() || !IsValidTarget(CurrentTarget.Get()))
        {
            CurrentTarget = nullptr; 
            FindNewTarget(); // This function already has logging
        }

        // Calculate instantaneous InterceptPoint or TargetLocation
        FVector InstantaneousTargetPoint;
        float TimeToIntercept = -1.0f; // To store the time from prediction

        if (CurrentTarget.IsValid())
        {
            FVector TargetLocation = CurrentTarget->GetActorLocation();
            FVector TargetVelocity = CurrentTarget->GetVelocity(); 
            FVector ShooterLocation = GetMuzzleLocation();
            FVector ShooterVelocity = GetShooterVelocity();

            // Log inputs to CalculateInterceptPoint (optional, can be verbose)
            // UE_LOG(LogSolaraqTurret, Warning, TEXT("InterceptCalc INPUTS: ShooterLoc: %s, ShooterVel: %s, TargetLoc: %s, TargetVel: %s, ProjSpeed: %.1f"),
            //     *ShooterLocation.ToString(), *ShooterVelocity.ToString(), *TargetLocation.ToString(), *TargetVelocity.ToString(), ActualProjectileSpeed);

            bool bCanIntercept = USolaraqMathLibrary::CalculateInterceptPoint(
                ShooterLocation, ShooterVelocity,
                TargetLocation, TargetVelocity,
                ActualProjectileSpeed, InstantaneousTargetPoint, TimeToIntercept // Pass reference for TimeToIntercept
            );
            
            if (bCanIntercept)
            {
                const float MaxReliablePredictionTime = 3.0f; // Tunable: Max seconds into future to trust prediction fully
                if (TimeToIntercept > MaxReliablePredictionTime)
                {
                    UE_LOG(LogSolaraqTurret, Warning, TEXT("InterceptCalc: TimeToIntercept (%.2fs) > MaxReliable (%.2fs). Capping aim point based on current velocity."), TimeToIntercept, MaxReliablePredictionTime);
                    // Aim where target would be in MaxReliablePredictionTime seconds at its current velocity
                    InstantaneousTargetPoint = TargetLocation + TargetVelocity * MaxReliablePredictionTime;
                }
                // else: TimeToIntercept is reasonable, use the calculated InstantaneousTargetPoint from CalculateInterceptPoint
                 UE_LOG(LogSolaraqTurret, Warning, TEXT("InterceptCalc SUCCEEDED: Original InstantTargetPt: %s, Time: %.2fs"), *InstantaneousTargetPoint.ToString(), TimeToIntercept);
            }
            else
            {
                // Prediction failed, InstantaneousTargetPoint is already TargetLocation by the CalculateInterceptPoint function's fallback
                 UE_LOG(LogSolaraqTurret, Warning, TEXT("InterceptCalc FAILED: Fallback InstantTargetPt (TargetLocation): %s"), *InstantaneousTargetPoint.ToString());
            }
        }
        else // No valid current target
        {
            // If no target, turret might return to a default orientation.
            // For now, let SmoothedAimWorldLocation try to persist, or aim "forward".
            // Using current SmoothedAimWorldLocation to avoid snapping if target is briefly lost.
            InstantaneousTargetPoint = SmoothedAimWorldLocation; 
            // Alternative if you want it to aim "forward" from the base when no target:
            // if (GetOwner()) { InstantaneousTargetPoint = GetMuzzleLocation() + GetOwner()->GetActorForwardVector() * TargetingRange * 0.5f; }
            // else { InstantaneousTargetPoint = GetMuzzleLocation() + GetActorForwardVector() * TargetingRange * 0.5f; }
        }

        // Smoothly interpolate SmoothedAimWorldLocation towards the (potentially capped) InstantaneousTargetPoint
        SmoothedAimWorldLocation = FMath::VInterpTo(SmoothedAimWorldLocation, InstantaneousTargetPoint, DeltaTime, AimSmoothingSpeed);
        
        UE_LOG(LogSolaraqTurret, Warning, TEXT("SERVER Turret %s: Final Processed InstantTargetPt: %s, SmoothedAimPt: %s"),
            *GetName(), *InstantaneousTargetPoint.ToString(), *SmoothedAimWorldLocation.ToString());

        // Rotate turret towards the SMOOTHED aim point
        RotateTurretTowards(SmoothedAimWorldLocation, DeltaTime); // This function has detailed rotation logs

        // Attempt to fire if cooldown is ready and aimed (still aims based on SmoothedAimWorldLocation for firing decision)
        if (CurrentTarget.IsValid() && FireCooldownRemaining <= 0.0f)
        {
            // UE_LOG(LogSolaraqTurret, Warning, TEXT("SERVER Turret %s: Cooldown ready. Attempting fire (aiming at SmoothedAimWorldLocation: %s)."), *GetName(), *SmoothedAimWorldLocation.ToString());
            AttemptFire(SmoothedAimWorldLocation); // Fire towards the smoothed point
        }
    }
    else // Client handles smooth visual rotation
    {
        if (TurretYawPivot)
        {
            FRotator CurrentLocalVisualRotation = TurretYawPivot->GetRelativeRotation();
            // ClientTargetYawPivotRelativeRotation is updated by OnRep_ReplicatedYawPivotRelativeRotation
            FRotator NewVisualRotation = FMath::RInterpTo(CurrentLocalVisualRotation, ClientTargetYawPivotRelativeRotation, DeltaTime, TurnRateDegreesPerSecond * 1.5f); 
            TurretYawPivot->SetRelativeRotation(NewVisualRotation);

            UE_LOG(LogSolaraqTurret, Warning, TEXT("CLIENT Turret %s: Tick: Current Visual Yaw: %.2f, Target Visual Yaw (from OnRep): %.2f, New Visual Yaw: %.2f"), 
                *GetName(), 
                CurrentLocalVisualRotation.Yaw,
                ClientTargetYawPivotRelativeRotation.Yaw,
                NewVisualRotation.Yaw);
        }
    }
}

void ATurretBase::OnRep_ReplicatedYawPivotRelativeRotation()
{
    // This function is called on clients when ReplicatedYawPivotRelativeRotation changes.
    // We update the client's *target* for its own interpolation.
    ClientTargetYawPivotRelativeRotation = ReplicatedYawPivotRelativeRotation;
    // UE_LOG(LogSolaraqTurret, Warning, TEXT("Client Turret %s: OnRep_ReplicatedYawPivotRelativeRotation. New Target Visual Rotation: %s"), *GetName(), *ClientTargetYawPivotRelativeRotation.ToString());

    // If you want immediate snap on client instead of client-side interp, do this:
    // if (TurretYawPivot) TurretYawPivot->SetRelativeRotation(ReplicatedYawPivotRelativeRotation);
}


void ATurretBase::RotateTurretTowards(const FVector& TargetWorldLocation, float DeltaTime)
{
    if (!HasAuthority()) return; 

    if (!TurretYawPivot || !TurretYawPivot->GetAttachParent() || !MuzzleLocationComponent)
    {
        UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: RotateTurretTowards: Missing components. Aborting rotation."), *GetName());
        return;
    }

    // 1. Get the desired yaw in degrees from atan2 (range -180 to 180)
    float DesiredRelativeYawDegreesAtan2 = GetDesiredYawRelativeToBase(TargetWorldLocation);

    // (Clamping logic remains the same if you have yaw limits)
    if (MaxYawRotationAngleDegrees > KINDA_SMALL_NUMBER && MaxYawRotationAngleDegrees < (180.0f - KINDA_SMALL_NUMBER))
    {
        DesiredRelativeYawDegreesAtan2 = FMath::Clamp(DesiredRelativeYawDegreesAtan2, -MaxYawRotationAngleDegrees, MaxYawRotationAngleDegrees);
    }

    FRotator CurrentRelativeRotation = TurretYawPivot->GetRelativeRotation();
    float CurrentYawDegrees = CurrentRelativeRotation.Yaw;

    // 2. Unwrap the target yaw to be closer to the current yaw
    // This helps prevent RInterpTo from choosing the "long way around" if atan2 output flips sign
    // when the target crosses the -180/180 boundary relative to the turret's current orientation.
    float TargetYawDegreesContinuous = DesiredRelativeYawDegreesAtan2;

    // If the shortest angle between current and atan2 target is more than 180 degrees the "wrong way"
    // adjust the atan2 target by 360 to make it continuous.
    if (FMath::Abs(TargetYawDegreesContinuous - CurrentYawDegrees) > 180.0f)
    {
        if (TargetYawDegreesContinuous > CurrentYawDegrees)
        {
            TargetYawDegreesContinuous -= 360.0f;
        }
        else
        {
            TargetYawDegreesContinuous += 360.0f;
        }
    }
    // One more check: if after unwrapping, it's still far due to multiple wraps (less likely for single frame change)
    // This is a simpler form of finding the delta and choosing the smallest one.
    // A more robust way:
    // float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYawDegrees, DesiredRelativeYawDegreesAtan2);
    // float TargetYawForInterp = CurrentYawDegrees + DeltaYaw;
    // For now, the above unwrap should help with the single large jump.
    // Let's use FMath::FindDeltaAngleDegrees for a cleaner approach:

    float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYawDegrees, DesiredRelativeYawDegreesAtan2);
    float TargetYawForInterp = CurrentYawDegrees + DeltaYaw;


    // Construct the target rotation for RInterpTo
    FRotator TargetRelativeRotationForInterp = FRotator(CurrentRelativeRotation.Pitch, TargetYawForInterp, CurrentRelativeRotation.Roll);

    UE_LOG(LogSolaraqTurret, Warning, TEXT("SERVER Turret %s: RotateTurretTowards: CurrentRelYaw: %.2f, atan2Desired: %.2f, DeltaYaw: %.2f, TargetForInterp: %.2f"),
        *GetName(),
        CurrentYawDegrees,
        DesiredRelativeYawDegreesAtan2,
        DeltaYaw,
        TargetYawForInterp);

    // Interpolate smoothly
    FRotator NewRelativeRotation = FMath::RInterpTo(
        CurrentRelativeRotation,
        TargetRelativeRotationForInterp, 
        DeltaTime,
        TurnRateDegreesPerSecond 
    );

    TurretYawPivot->SetRelativeRotation(NewRelativeRotation);
    ReplicatedYawPivotRelativeRotation = NewRelativeRotation; 
}

void ATurretBase::AttemptFire(const FVector& AimLocation)
{
    if (!bCanFire || FireCooldownRemaining > 0.0f || !ProjectileClass || !GetWorld() || !MuzzleLocationComponent)
    {
        // UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: AttemptFire prerequisites not met... Aborting fire."), *GetName());
        return;
    }

    if (!HasAuthority()) 
    {
        return;
    }
    
    // Ensure ActualProjectileSpeed is valid before firing
    if (ActualProjectileSpeed <= 0.0f)
    {
        UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: AttemptFire: ActualProjectileSpeed is %.2f, cannot fire projectile with valid speed. Aborting."), *GetName(), ActualProjectileSpeed);
        return;
    }

    const FVector MuzzleLoc = GetMuzzleLocation();
    const FRotator MuzzleRot = GetMuzzleRotation(); 

    FVector DirectionToPredictedTarget = (AimLocation - MuzzleLoc).GetSafeNormal();
    FVector CurrentGunDirection = MuzzleRot.Vector(); 

    DirectionToPredictedTarget.Normalize();
    CurrentGunDirection.Normalize();

    float DotProduct = FVector::DotProduct(CurrentGunDirection, DirectionToPredictedTarget);
    DotProduct = FMath::Clamp(DotProduct, -1.0f, 1.0f);
    float AngleToTargetDegrees = FMath::RadiansToDegrees(FMath::Acos(DotProduct));

    if (AngleToTargetDegrees <= FiringToleranceAngleDegrees)
    {
        UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: AIMED! Firing projectile."), *GetName());
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = GetOwner() ? GetOwner() : this; 
        APawn* InstigatingPawn = Cast<APawn>(GetOwner());
        if(!InstigatingPawn && GetOwner()) 
        {
            InstigatingPawn = Cast<APawn>(GetOwner()->GetOwner());
        }
        SpawnParams.Instigator = InstigatingPawn ? InstigatingPawn : Cast<APawn>(this); 

        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        ASolaraqProjectile* SpawnedProjectile = GetWorld()->SpawnActor<ASolaraqProjectile>(
            ProjectileClass,
            MuzzleLoc,
            MuzzleRot,
            SpawnParams
        );

        if (SpawnedProjectile)
        {
            // --- SET PROJECTILE SPEED ---
            UProjectileMovementComponent* ProjMoveComp = SpawnedProjectile->GetProjectileMovementComponent();
            if (ProjMoveComp)
            {
                ProjMoveComp->InitialSpeed = ActualProjectileSpeed;
                ProjMoveComp->MaxSpeed = ActualProjectileSpeed; // Good to keep these consistent
                // ProjMoveComp->Velocity = MuzzleRot.Vector() * ActualProjectileSpeed; // Alternative: Set velocity directly
                // ProjMoveComp->UpdateComponentVelocity(); // If using direct velocity set
                ProjMoveComp->Activate(true); // Ensure component is active and uses new speed
                UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: Set projectile %s InitialSpeed to %.2f"), *GetName(), *SpawnedProjectile->GetName(), ActualProjectileSpeed);
            }
            else
            {
                UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: Spawned projectile %s has NO ProjectileMovementComponent! Cannot set speed."), *GetName(), *SpawnedProjectile->GetName());
            }
            // --- END SET PROJECTILE SPEED ---
            
            UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: Successfully Fired Projectile %s towards %s."), *GetName(), *SpawnedProjectile->GetName(), *AimLocation.ToString());
            
            FireCooldownRemaining = 1.0f / FireRate;

            if (MuzzleFlashEffect)
            {
                UGameplayStatics::SpawnEmitterAttached(MuzzleFlashEffect, MuzzleLocationComponent, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
            }
            if (FireSound)
            {
                UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLoc);
            }
        }
        else
        {
            UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: FAILED to spawn projectile from class %s!"), *GetName(), *GetNameSafe(ProjectileClass));
        }
    }
    // else
    // {
    //    UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: Holding fire. Angle to target %.2f > Tolerance %.2f"), *GetName(), AngleToTargetDegrees, FiringToleranceAngleDegrees);
    // }
}

// ... (rest of the functions: GetGenericTeamId, GetTeamAttitudeTowards, SetTargetManually, FindNewTarget, IsValidTarget, GetDesiredYawRelativeToBase, GetShooterVelocity, GetMuzzleLocation, GetMuzzleRotation remain the same as your last provided version with logs)
// Make sure they are included below this point. I'm omitting them for brevity as they didn't need direct changes for these specific issues, beyond the logging already added.
// Ensure the Get... functions are still there:
FGenericTeamId ATurretBase::GetGenericTeamId() const
{
    if (AActor* MyOwner = GetOwner())
    {
        if (IGenericTeamAgentInterface* OwnerTeamAgent = Cast<IGenericTeamAgentInterface>(MyOwner))
        {
            return OwnerTeamAgent->GetGenericTeamId();
        }
        if(APawn* OwnerPawn = Cast<APawn>(MyOwner))
        {
            if(OwnerPawn->GetController() && OwnerPawn->GetController()->Implements<UGenericTeamAgentInterface>())
            {
                return Cast<IGenericTeamAgentInterface>(OwnerPawn->GetController())->GetGenericTeamId();
            }
        }
    }
    return TeamId; 
}

ETeamAttitude::Type ATurretBase::GetTeamAttitudeTowards(const AActor& Other) const
{
    ETeamAttitude::Type Attitude = ETeamAttitude::Neutral; // Default
    if (const APawn* OtherPawn = Cast<const APawn>(&Other))
    {
        if (OtherPawn->GetController())
        {
            if (const IGenericTeamAgentInterface* TeamAgentController = Cast<const IGenericTeamAgentInterface>(OtherPawn->GetController()))
            {
                Attitude = FGenericTeamId::GetAttitude(GetGenericTeamId(), TeamAgentController->GetGenericTeamId());
            }
        }
        else if (const IGenericTeamAgentInterface* TeamAgentPawn = Cast<const IGenericTeamAgentInterface>(OtherPawn)) // Check pawn if controller has no team
        {
             Attitude = FGenericTeamId::GetAttitude(GetGenericTeamId(), TeamAgentPawn->GetGenericTeamId());
        }
    }
    else if (const IGenericTeamAgentInterface* TeamAgent = Cast<const IGenericTeamAgentInterface>(&Other))
    {
         Attitude = FGenericTeamId::GetAttitude(GetGenericTeamId(), TeamAgent->GetGenericTeamId());
    }
    // UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: Attitude towards %s is %d."), *GetName(), *Other.GetName(), static_cast<int32>(Attitude));
    return Attitude;
}


void ATurretBase::SetTargetManually(AActor* NewTarget)
{
    UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: SetTargetManually called with %s."), *GetName(), *GetNameSafe(NewTarget));
    if (NewTarget && IsValidTarget(NewTarget))
    {
        CurrentTarget = NewTarget;
        UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: Manual target %s set."), *GetName(), *NewTarget->GetName());
    }
    else
    {
        CurrentTarget = nullptr;
        UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: Manual target %s was invalid or null. Target cleared."), *GetName(), *GetNameSafe(NewTarget));
    }
}

void ATurretBase::FindNewTarget()
{
    UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: FindNewTarget START."), *GetName());
    if (!GetWorld() || !bCanFire || !MuzzleLocationComponent)
    {
        UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: FindNewTarget prerequisites not met (World: %d, bCanFire: %d, Muzzle: %d). Aborting."),
            *GetName(), GetWorld() != nullptr, bCanFire, MuzzleLocationComponent != nullptr);
        return;
    }

    AActor* BestTarget = nullptr;
    float MinDistanceSq = FMath::Square(TargetingRange);

    TArray<AActor*> OverlappingActors;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn)); 

    FCollisionShape SphereShape = FCollisionShape::MakeSphere(TargetingRange);
    FVector TurretLocation = GetActorLocation(); 
    UKismetSystemLibrary::SphereOverlapActors(
        GetWorld(),
        TurretLocation,
        TargetingRange,
        ObjectTypes,
        nullptr, 
        TArray<AActor*>({this, GetOwner()}), 
        OverlappingActors
    );
    UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: SphereOverlap found %d actors."), *GetName(), OverlappingActors.Num());

    for (AActor* PotentialTarget : OverlappingActors)
    {
        if (!PotentialTarget) continue;
        if (IsValidTarget(PotentialTarget))
        {
            float DistSq = FVector::DistSquared(GetMuzzleLocation(), PotentialTarget->GetActorLocation());
            if (DistSq < MinDistanceSq) 
            {
                if (MaxYawRotationAngleDegrees > 0.0f && MaxYawRotationAngleDegrees < 180.0f)
                {
                    float AngleToTargetYaw = GetDesiredYawRelativeToBase(PotentialTarget->GetActorLocation());
                    if (FMath::Abs(AngleToTargetYaw) > MaxYawRotationAngleDegrees)
                    {
                        continue; 
                    }
                }
                MinDistanceSq = DistSq;
                BestTarget = PotentialTarget;
            }
        }
    }
    CurrentTarget = BestTarget;
    if (BestTarget)
    {
        UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: Acquired target: %s (Dist: %.0f)"), *GetName(), *BestTarget->GetName(), FMath::Sqrt(MinDistanceSq));
    }
    else
    {
         UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: No valid targets found in range/arc."), *GetName());
    }
    UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: FindNewTarget END."), *GetName());
}

bool ATurretBase::IsValidTarget(AActor* TargetActor) const
{
    if (!TargetActor || TargetActor == this || (GetOwner() && TargetActor == GetOwner()))
    {
        return false;
    }

    const ASolaraqShipBase* ShipTarget = Cast<const ASolaraqShipBase>(TargetActor);
    if (ShipTarget && ShipTarget->IsDead())
    {
        return false;
    }
    
    ETeamAttitude::Type Attitude = GetTeamAttitudeTowards(*TargetActor);
    if (Attitude != ETeamAttitude::Hostile)
    {
        return false;
    }
    return true;
}

float ATurretBase::GetDesiredYawRelativeToBase(const FVector& TargetWorldLocation) const
{
    if (!TurretYawPivot || !TurretYawPivot->GetAttachParent() || !MuzzleLocationComponent)
    {
        UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: GetDesiredYawRelativeToBase: Missing components. Returning 0."), *GetName());
        return 0.0f;
    }

    const FVector DirectionToTargetWorld = (TargetWorldLocation - GetMuzzleLocation()).GetSafeNormal();
    const FTransform BaseTransform = TurretYawPivot->GetAttachParent()->GetComponentTransform();
    const FVector DirectionToTargetLocalToBase = BaseTransform.InverseTransformVectorNoScale(DirectionToTargetWorld);
    float DesiredYaw = FMath::RadiansToDegrees(FMath::Atan2(DirectionToTargetLocalToBase.Y, DirectionToTargetLocalToBase.X));
    
    return DesiredYaw;
}

FVector ATurretBase::GetShooterVelocity() const
{
    if (ASolaraqShipBase* OwningShip = Cast<ASolaraqShipBase>(GetOwner()))
    {
        if (USceneComponent* PhysicsRoot = OwningShip->GetCollisionAndPhysicsRoot()) 
        {
            if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(PhysicsRoot))
            {
                 return PrimComp->GetPhysicsLinearVelocity();
            }
        }
        return OwningShip->GetVelocity(); 
    }
    if (APawn* OwningPawn = Cast<APawn>(GetOwner()))
    {
        return OwningPawn->GetVelocity();
    }
    if (AActor* MyOwner = GetOwner())
    {
        return MyOwner->GetVelocity(); 
    }
    return FVector::ZeroVector;
}

FVector ATurretBase::GetMuzzleLocation() const
{
    if (MuzzleLocationComponent) return MuzzleLocationComponent->GetComponentLocation();
    if (TurretGunMesh) return TurretGunMesh->GetComponentLocation(); // Fallback if MuzzleLocation not set up
    if (TurretYawPivot) return TurretYawPivot->GetComponentLocation(); // Further fallback
    UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: GetMuzzleLocation falling back to GetActorLocation() as MuzzleLocationComponent is missing/invalid!"), *GetName());
    return GetActorLocation();
}

FRotator ATurretBase::GetMuzzleRotation() const
{
    if (MuzzleLocationComponent) return MuzzleLocationComponent->GetComponentRotation();
    if (TurretGunMesh) return TurretGunMesh->GetComponentRotation(); // Fallback
    if (TurretYawPivot) return TurretYawPivot->GetComponentRotation(); // Further fallback
    UE_LOG(LogSolaraqTurret, Warning, TEXT("Turret %s: GetMuzzleRotation falling back to GetActorRotation() as MuzzleLocationComponent is missing/invalid!"), *GetName());
    return GetActorRotation();
}