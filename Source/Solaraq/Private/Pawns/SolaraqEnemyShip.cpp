// SolaraqEnemyShip.cpp

#include "Pawns/SolaraqEnemyShip.h"
#include "Logging/SolaraqLogChannels.h" // Your custom log channel
#include "Components/BoxComponent.h" // For physics root access
#include "Components/SphereComponent.h"

ASolaraqEnemyShip::ASolaraqEnemyShip()
{
    // Constructor code if needed (e.g., setting default values different from base)
    UE_LOG(LogSolaraqGeneral, Verbose, TEXT("ASolaraqEnemyShip %s Constructed"), *GetName());
     FireRate = FMath::RandRange(0.4f, 0.8f); // Example: Randomize fire rate slightly per instance
}

void ASolaraqEnemyShip::BeginPlay()
{
    Super::BeginPlay();
    LastFireTime = -FireRate; // Allow firing immediately
}

void ASolaraqEnemyShip::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    // Any specific Tick logic for the enemy ship itself (not controller logic)
}


void ASolaraqEnemyShip::TurnTowards(const FVector& TargetLocation)
{
    // Needs to only run on the server as rotation is replicated.
    if (!HasAuthority()) return;
    if (!CollisionAndPhysicsRoot || IsDead() || !CollisionAndPhysicsRoot->IsSimulatingPhysics()) return;

    const FVector CurrentLocation = GetActorLocation();
    const FVector DirectionToTarget = (TargetLocation - CurrentLocation).GetSafeNormal();
    if (DirectionToTarget.IsNearlyZero()) return;

    FRotator TargetRotation = DirectionToTarget.Rotation();
    FRotator CurrentRotation = CollisionAndPhysicsRoot->GetComponentRotation(); // Use physics rotation

    // UE_LOG(LogSolaraqAI, Verbose, TEXT("TurnTowards --- CurrentRot(Physics): %s, TargetRot(World): %s"), *CurrentRotation.ToString(), *TargetRotation.ToString()); // Optional Log

    float CurrentYaw = CurrentRotation.Yaw;
    float TargetYaw = TargetRotation.Yaw;
    float YawDifference = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);

    // --- Stop applying torque when very close to the target angle ---
    // Tune this threshold (degrees) - Start maybe a bit higher like 2.0 or 3.0
    if (FMath::Abs(YawDifference) < 2.0f)
    {
        // Optionally reduce/zero out angular velocity when very close to target angle
        FVector CurrentAngularVel = CollisionAndPhysicsRoot->GetPhysicsAngularVelocityInDegrees();
        if(FMath::Abs(CurrentAngularVel.Z) > 1.0f) // Only dampen if spinning significantly
        {
            // Apply damping faster when close to target
            CollisionAndPhysicsRoot->SetPhysicsAngularVelocityInDegrees(FVector(CurrentAngularVel.X, CurrentAngularVel.Y, CurrentAngularVel.Z * 0.5f));
            UE_LOG(LogSolaraqAI, Warning, TEXT("%s TurnTowards: Yaw difference small (%.2f). Applying damping to AngVelZ: %.2f"), *GetName(), YawDifference, CurrentAngularVel.Z * 0.5f);
        }
        else {
             UE_LOG(LogSolaraqAI, Warning, TEXT("%s TurnTowards: Yaw difference small (%.2f) and AngVelZ low. No torque/damping."), *GetName(), YawDifference);
        }
        return; // Don't apply positive torque if already aligned
    }

    // --- Torque Calculation ---
    // Start with the MaxTurnTorque value that was previously causing overshoot
    float MaxTurnTorque = 10000.0f; // <<< YOUR PREVIOUSLY TUNED VALUE (The one that was too fast)
    float TurnDirection = FMath::Sign(YawDifference);

    // --- Proportional Torque Calculation ---
    // Angle (degrees) at which to start reducing torque. Tune this value!
    // Start larger (e.g., 90) and decrease if it still overshoots.
    float SlowdownAngle = 90.0f; // <<< TUNABLE (Try 180, 90, 60, 45, 30...)
    // Minimum factor prevents zero torque when far from target. Tune if needed (0.05 - 0.2 range usually ok)
    float MinTorqueFactor = 0.1f; // <<< TUNABLE

    // Calculate scaling factor: 1.0 when angle diff >= SlowdownAngle, decreasing linearly to MinTorqueFactor.
    float TorqueFactor = FMath::Clamp(FMath::Abs(YawDifference) / SlowdownAngle, MinTorqueFactor, 1.0f);
    float TorqueMagnitude = MaxTurnTorque * TorqueFactor;
    // --- End Proportional Torque ---

    FVector TorqueToApply = FVector(0.f, 0.f, TurnDirection * TorqueMagnitude);
    // Keep AccelChange=true for now unless you have specific reasons to use mass
    CollisionAndPhysicsRoot->AddTorqueInDegrees(TorqueToApply, NAME_None, true);

    UE_LOG(LogSolaraqAI, Warning, TEXT("%s TurnTowards: YawDiff: %.2f, Factor: %.2f, Applying Torque: %.2f"), *GetName(), YawDifference, TorqueFactor, TorqueToApply.Z);
}

void ASolaraqEnemyShip::FireWeapon()
{
    // Needs to only run on the server where weapon logic is authoritative.
    if (!HasAuthority()) return;
    if (IsDead()) return;

    // --- Check Cooldown ---
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime < LastFireTime + FireRate)
    {
        // UE_LOG(LogSolaraqAI, VeryVerbose, TEXT("%s FireWeapon: Cooldown Active (%.2f / %.2f)"), *GetName(), CurrentTime - LastFireTime, FireRate);
        return; // Still on cooldown
    }

    // --- Check if Projectile Class is Assigned ---
    if (!ProjectileClass)
    {
        UE_LOG(LogSolaraqCombat, Error, TEXT("%s FireWeapon: ProjectileClass is NULL! Assign 'BP_Bullet' in the Enemy Ship Blueprint defaults."), *GetName());
        return; // Cannot fire without a projectile type
    }

    // --- Get World ---
    UWorld* const World = GetWorld();
    if (!World)
    {
         UE_LOG(LogSolaraqCombat, Error, TEXT("%s FireWeapon: GetWorld() returned NULL!"), *GetName());
         return; // Cannot spawn without a world context
    }

    // --- Calculate Spawn Transform ---
    const FVector ForwardVector = GetActorForwardVector();
    const FVector SpawnLocation = GetActorLocation() + (ForwardVector * MuzzleOffset); // Use MuzzleOffset property
    const FRotator SpawnRotation = ForwardVector.Rotation(); // Projectile usually faces ship's forward direction

    UE_LOG(LogSolaraqCombat, Warning, TEXT("%s FireWeapon: Attempting to spawn %s at %s | %s"),
        *GetName(), *ProjectileClass->GetName(), *SpawnLocation.ToString(), *SpawnRotation.ToString());

    // --- Set Spawn Parameters ---
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this; // The ship that fired it
    SpawnParams.Instigator = this; // The Pawn is the instigator causing the damage etc.
    // AdjustIfPossibleButAlwaysSpawn is generally safe for projectiles unless you *want* them blocked by immediate overlaps
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // --- Spawn the Projectile ---
    AActor* SpawnedProjectile = World->SpawnActor<AActor>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);

    // --- Check Spawn Result & Reset Cooldown ---
    if (!SpawnedProjectile)
    {
         UE_LOG(LogSolaraqCombat, Error, TEXT("%s FireWeapon: World->SpawnActor failed for %s!"), *GetName(), *ProjectileClass->GetName());
    }
    else
    {
         UE_LOG(LogSolaraqCombat, Warning, TEXT("%s FireWeapon: Spawned %s successfully."), *GetName(), *SpawnedProjectile->GetName());
         LastFireTime = CurrentTime; // IMPORTANT: Reset cooldown ONLY on successful spawn
    }
}

void ASolaraqEnemyShip::RequestMoveForward(float Value)
{
     // --- Basic Implementation ---
     // AI needs to call the Server RPC directly, as it doesn't have player input bindings.
     if (!HasAuthority()) // Should ideally only be called on server instance anyway
     {
          // AI on client shouldn't directly try to move the server instance
         return;
     }
      if(IsDead()) return;

     // Directly call the movement processing logic (or the Server RPC if that contains more logic)
     // Since ProcessMoveForwardInput is protected in base, we can call it if needed,
     // OR just call the Server RPC which is public. Calling the RPC is safer if we
     // later add client-side prediction that relies on the RPC flow.
     // For now, let's assume ProcessMoveForwardInput is the core server logic.
     ProcessMoveForwardInput(Value); // Call protected base class function


     // If ProcessMoveForwardInput was purely for client->server, and server logic is elsewhere:
     // Server_SendMoveForwardInput(Value); // Call the Server RPC if needed

     UE_LOG(LogSolaraqAI, VeryVerbose, TEXT("%s AI RequestMoveForward: %.2f"), *GetName(), Value);
}