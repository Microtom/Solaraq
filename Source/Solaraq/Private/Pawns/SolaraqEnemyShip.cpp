// SolaraqEnemyShip.cpp

#include "Pawns/SolaraqEnemyShip.h"
#include "Logging/SolaraqLogChannels.h" // Your custom log channel
#include "Components/BoxComponent.h" // For physics root access
#include "Components/SphereComponent.h"

ASolaraqEnemyShip::ASolaraqEnemyShip()
{
    // Constructor code if needed (e.g., setting default values different from base)
    //UE_LOG(LogSolaraqAI, Warning, TEXT("ASolaraqEnemyShip %s Constructed"), *GetName());
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

void ASolaraqEnemyShip::HandleDestruction()
{
    // Should only be called on the Server
    if (!HasAuthority() || bIsDead)
    {
        return;
    }

    UE_LOG(LogSolaraqCombat, Log, TEXT("Enemy Ship %s Destroyed!"), *GetName());

    // --- Spawn Loot ---
  /*  if (LootPickupClass && GetWorld())
    {
        int32 NumDrops = FMath::RandRange(MinLootDrops, MaxLootDrops);
        if (NumDrops > 0)
        {
            UE_LOG(LogSolaraqSystem, Log, TEXT("Spawning %d loot drops from %s"), NumDrops, *GetName());
            const FVector DeathLocation = GetActorLocation();
            const FRotator SpawnRotation = FRotator::ZeroRotator; // Pickup rotation doesn't matter much

            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = nullptr; // Loot isn't "owned" by the dead ship
            SpawnParams.Instigator = this; // The ship that died is the instigator
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

            for (int i = 0; i < NumDrops; ++i)
            {
                // Calculate random offset location
                FVector SpawnOffset = FMath::VRand() * FMath::FRandRange(0.f, LootSpawnRadius); // Get random direction vector
                SpawnOffset.Z = 0; // Keep on XY plane
                FVector SpawnLocation = DeathLocation + SpawnOffset;

                ASolaraqPickupBase* SpawnedPickup = GetWorld()->SpawnActor<ASolaraqPickupBase>(LootPickupClass, SpawnLocation, SpawnRotation, SpawnParams);

                if (SpawnedPickup)
                {
                    UE_LOG(LogSolaraqSystem, Verbose, TEXT(" -> Spawned %s at %s"), *SpawnedPickup->GetName(), *SpawnLocation.ToString());
                    // Note: Dispersal impulse is handled in the pickup's BeginPlay
                }
                else
                {
                     UE_LOG(LogSolaraqSystem, Error, TEXT(" -> Failed to spawn LootPickupClass at %s!"), *SpawnLocation.ToString());
                }
            }
        }
    }
    // --- End Spawn Loot ---
*/

    // 1. Set the dead state (this will replicate via OnRep_IsDead)
    bIsDead = true;

    // 2. Immediately trigger visual/audio effects on all clients via Multicast
    Multicast_PlayDestructionEffects();

    // 3. Disable ship functionality on the server
    // ... (existing code: stop physics, disable collision, unpossess, etc.) ...
    if (CollisionAndPhysicsRoot)
    {
        CollisionAndPhysicsRoot->SetSimulatePhysics(false);
        CollisionAndPhysicsRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);
        CollisionAndPhysicsRoot->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }
    SetActorTickEnabled(false); // Stop ticking
    SetActorEnableCollision(ECollisionEnabled::NoCollision);
     if (CollisionAndPhysicsRoot) CollisionAndPhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
     if (ShipMeshComponent) ShipMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

     AController* CurrentController = GetController();
     if (CurrentController)
     {
        CurrentController->UnPossess();
     }

    // 4. Set the actor to be destroyed after a delay
    SetLifeSpan(5.0f); // Actor will be automatically destroyed after 5 seconds
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

    //UE_LOG(LogSolaraqAI, Warning, TEXT("TurnTowards --- CurrentRot(Physics): %s, TargetRot(World): %s"), *CurrentRotation.ToString(), *TargetRotation.ToString()); // Optional Log

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
            //UE_LOG(LogSolaraqAI, Warning, TEXT("%s TurnTowards: Yaw difference small (%.2f). Applying damping to AngVelZ: %.2f"), *GetName(), YawDifference, CurrentAngularVel.Z * 0.5f);
        }
        else {
             //UE_LOG(LogSolaraqAI, Warning, TEXT("%s TurnTowards: Yaw difference small (%.2f) and AngVelZ low. No torque/damping."), *GetName(), YawDifference);
        }
        return; // Don't apply positive torque if already aligned
    }

    // --- Torque Calculation ---
    // Start with the MaxTurnTorque value that was previously causing overshoot
    float MaxTurnTorque = 3000.0f; // <<< YOUR PREVIOUSLY TUNED VALUE (The one that was too fast)
    float TurnDirection = FMath::Sign(YawDifference);

    // --- Proportional Torque Calculation ---
    // Angle (degrees) at which to start reducing torque. Tune this value!
    // Start larger (e.g., 90) and decrease if it still overshoots.
    float SlowdownAngle = 90.0f; // <<< TUNABLE (Try 180, 90, 60, 45, 30...)
    // Minimum factor prevents zero torque when far from target. Tune if needed (0.05 - 0.2 range usually ok)
    float MinTorqueFactor = 0.5f; // <<< TUNABLE

    // Calculate scaling factor: 1.0 when angle diff >= SlowdownAngle, decreasing linearly to MinTorqueFactor.
    float TorqueFactor = FMath::Clamp(FMath::Abs(YawDifference) / SlowdownAngle, MinTorqueFactor, 1.0f);
    float TorqueMagnitude = MaxTurnTorque * TorqueFactor;
    // --- End Proportional Torque ---

    FVector TorqueToApply = FVector(0.f, 0.f, TurnDirection * TorqueMagnitude);
    // Keep AccelChange=true for now unless you have specific reasons to use mass
    CollisionAndPhysicsRoot->AddTorqueInDegrees(TorqueToApply, NAME_None, true);

    //UE_LOG(LogSolaraqAI, Warning, TEXT("%s TurnTowards: YawDiff: %.2f, Factor: %.2f, Applying Torque: %.2f"), *GetName(), YawDifference, TorqueFactor, TorqueToApply.Z);
}

void ASolaraqEnemyShip::FireWeapon()
{
    // Needs to only run on the server where weapon logic is authoritative.
    if (!HasAuthority()) return;
    if (IsDead()) return;

    Super::PerformFireWeapon();
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

     UE_LOG(LogSolaraqAI, Warning, TEXT("%s AI RequestMoveForward: %.2f"), *GetName(), Value);
}