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
#include "Net/UnrealNetwork.h"
#include "Projectiles/SolaraqHomingProjectile.h"
#include "Projectiles/SolaraqProjectile.h"
#include "Components/DockingPadComponent.h" // Include for docking logic
#include "TimerManager.h" // For potential future timed sequences
#include "Controllers/SolaraqPlayerController.h"

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

    NetUpdateFrequency = 160.0f; // Default is often 100, but depends on project. Try 30, 60.
    MinNetUpdateFrequency = 30.0f;
    
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
    const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    if (bIsDead || ActualDamage <= 0.0f)
    {
        return 0.0f;
    }

    if (HasAuthority())
    {
        CurrentHealth = FMath::Clamp(CurrentHealth - ActualDamage, 0.0f, MaxHealth);
        NET_LOG(LogSolaraqCombat, Log, TEXT("Took %.1f damage from %s. CurrentHealth: %.1f"), ActualDamage, *GetNameSafe(DamageCauser), CurrentHealth);

        if (CurrentHealth <= 0.0f)
        {
            HandleDestruction();
        }
    }
    return ActualDamage;
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
        bIsDead = false;
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
    // This function is called on the ship instance (could be client or server side initially
    // depending on where PlayerController::HandleInteractInput is called from).
    // For level transition, the authority (server for networked game, local for standalone) should handle it.
    // For simplicity in a single-player context, we can proceed.
    // If networked, this would likely be an RPC to the server.
    UE_LOG(LogSolaraqTransition, Warning, TEXT("Ship %s: RequestInteraction() called. CurrentDockingStatus: %s, ActiveDockingPad: %s"),
            *GetName(), *UEnum::GetValueAsString(CurrentDockingStatus), *GetNameSafe(ActiveDockingPad));
    
    if (IsShipDocked() && ActiveDockingPad) // Check if docked and to which pad
    {
        AController* CurrentShipController = GetController();
        
        if (!CurrentShipController)
        {
            UE_LOG(LogSolaraqTransition, Error, TEXT("Ship %s: GetController() returned NULL! Cannot transition."), *GetName());
            return;
        }

        UE_LOG(LogSolaraqTransition, Warning, TEXT("Ship %s: GetController() returned: %s (Class: %s). Attempting to cast to ASolaraqPlayerController."),
            *GetName(), *GetNameSafe(CurrentShipController), *GetNameSafe(CurrentShipController->GetClass()));

        
        ASolaraqPlayerController* PC = Cast<ASolaraqPlayerController>(CurrentShipController);
        if (PC)
        {
            UE_LOG(LogSolaraqTransition, Warning, TEXT("Ship %s: Successfully cast controller to ASolaraqPlayerController (%s). Interaction requested while docked to %s. Telling PC to transition."),
                *GetName(), *GetNameSafe(PC), *ActiveDockingPad->GetName());

            FName TargetLevelName = TEXT("CharacterTestLevel"); // Default character level name
            if (!CharacterLevelOverrideName.IsNone())
            {
                TargetLevelName = CharacterLevelOverrideName; // Use override if set
                UE_LOG(LogSolaraqTransition, Warning, TEXT("Ship %s: Using CharacterLevelOverrideName: %s"), *GetName(), *TargetLevelName.ToString());
            }
            else
            {
                UE_LOG(LogSolaraqTransition, Warning, TEXT("Ship %s: Using default TargetCharacterLevelName: %s"), *GetName(), *TargetLevelName.ToString());
            }
            
            // Get a unique identifier for the pad (e.g., its name or a custom tag)
            // This could be used by the character level's GameMode to spawn the player at a specific PlayerStart.
            FName PadID = ActiveDockingPad->GetFName(); 
            if (ActiveDockingPad->DockingPadUniqueID != NAME_None) // Prefer the UniqueID if set
            {
                PadID = ActiveDockingPad->DockingPadUniqueID;
                UE_LOG(LogSolaraqTransition, Warning, TEXT("Ship %s: Using DockingPadUniqueID for PadID: %s"), *GetName(), *PadID.ToString());
            }
            else
            {
                UE_LOG(LogSolaraqTransition, Warning, TEXT("Ship %s: DockingPadUniqueID is None for pad %s. Using pad's FName %s as PadID. Ensure PlayerStartTag matches this."), *GetName(), *ActiveDockingPad->GetName(), *PadID.ToString());
            }
            
            PC->InitiateLevelTransitionToCharacter(TargetLevelName, PadID);
        }
        else
        {
            UE_LOG(LogSolaraqTransition, Error, TEXT("Ship %s: Cast<ASolaraqPlayerController>(GetController()) FAILED. Controller was %s (Class: %s). Cannot transition."),
                *GetName(), *GetNameSafe(CurrentShipController), *GetNameSafe(CurrentShipController->GetClass()));
        }
    }
    else
    {
        UE_LOG(LogSolaraqTransition, Warning, TEXT("Ship %s: Interaction requested, but IsShipDocked() is %s or ActiveDockingPad is %s. Cannot transition."),
            *GetName(),
            IsShipDocked() ? TEXT("true") : TEXT("false"),
            ActiveDockingPad ? *ActiveDockingPad->GetName() : TEXT("NULL"));
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
        ActualDockingTargetRelativeRotation = (PadWorldTransform.GetRotation().Inverse() * FQuat(ShipWorldRotationAtDockStart)).Rotator();

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

void ASolaraqShipBase::PerformDockingAttachmentToPad(UDockingPadComponent* Pad)
{
    /*if (!HasAuthority() || !Pad || !CollisionAndPhysicsRoot) return;
    AActor* PadOwner = Pad->GetOwner();
    if (!PadOwner) return;

    NET_LOG(LogSolaraqSystem, Log, TEXT("Performing docking attachment to %s on %s."), *Pad->GetName(), *PadOwner->GetName());

    CollisionAndPhysicsRoot->SetSimulatePhysics(false);
    CollisionAndPhysicsRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);
    CollisionAndPhysicsRoot->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

    FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
    GetRootComponent()->AttachToComponent(Pad->GetAttachPoint(), AttachRules);
    GetRootComponent()->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
    NET_LOG(LogSolaraqSystem, Verbose, TEXT("Snapped to relative 0,0,0 of pad %s."), *Pad->GetName());

    Internal_DisableSystemsForDocking();*/
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

