// SolaraqEnemyShip.cpp

#include "Pawns/SolaraqEnemyShip.h"
#include "Logging/SolaraqLogChannels.h" // Your custom log channel
#include "Components/BoxComponent.h" // For physics root access
#include "Components/SphereComponent.h"

ASolaraqEnemyShip::ASolaraqEnemyShip()
{
    // Constructor code if needed (e.g., setting default values different from base)
    ////UE_LOG(LogSolaraqAI, Warning, TEXT("ASolaraqEnemyShip %s Constructed"), *GetName());
     FireRate = FMath::RandRange(0.1f, 0.2f); // Example: Randomize fire rate slightly per instance
     SpecificAITurnRate = 110.0f; // Default AI turn rate, can be adjusted in BP

    ThrustForce = 2800000.0f;       // e.g., Double the base thrust
    NormalMaxSpeed = 4000.0f;  
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

    //UE_LOG(LogSolaraqCombat, Log, TEXT("Enemy Ship %s Destroyed!"), *GetName());

    // --- Spawn Loot ---
  /*  if (LootPickupClass && GetWorld())
    {
        int32 NumDrops = FMath::RandRange(MinLootDrops, MaxLootDrops);
        if (NumDrops > 0)
        {
            //UE_LOG(LogSolaraqSystem, Log, TEXT("Spawning %d loot drops from %s"), NumDrops, *GetName());
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
                    //UE_LOG(LogSolaraqSystem, Verbose, TEXT(" -> Spawned %s at %s"), *SpawnedPickup->GetName(), *SpawnLocation.ToString());
                    // Note: Dispersal impulse is handled in the pickup's BeginPlay
                }
                else
                {
                     //UE_LOG(LogSolaraqSystem, Error, TEXT(" -> Failed to spawn LootPickupClass at %s!"), *SpawnLocation.ToString());
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


void ASolaraqEnemyShip::TurnTowards(const FVector& TargetLocation, float DeltaTime)
{
    // Needs to only run on the server as rotation is replicated.
    if (!HasAuthority() || IsDead() || DeltaTime <= KINDA_SMALL_NUMBER) // KINDA_SMALL_NUMBER to avoid issues with zero or negative delta time
    {
        return;
    }

    const FVector CurrentLocation = GetActorLocation();
    const FVector DirectionToTarget = (TargetLocation - CurrentLocation).GetSafeNormal();

    if (DirectionToTarget.IsNearlyZero())
    {
        // Already at target or invalid direction.
        // No turning needed. If physics were active and spinning the ship,
        // you might want to explicitly stop angular velocity here.
        // Since we're setting rotation directly, this isn't strictly necessary
        // unless an external physics force is still acting on it.
        return;
    }

    FRotator CurrentActorRotation = GetActorRotation();
    FRotator TargetWorldRotation = DirectionToTarget.Rotation();

    // We are in a 2.5D space game, so we only care about Yaw.
    // Pitch and Roll should be maintained (or set to 0 if ship should always be flat).
    // The base ship has physics constraints to lock X and Y rotation (Pitch and Roll).
    // So, we'll make the desired rotation only affect Yaw.
    FRotator DesiredRotation = FRotator(0.f, TargetWorldRotation.Yaw, 0.f); // Keep Pitch and Roll at 0.

    // Determine the turn rate to use
    float RotationRateToUse = SpecificAITurnRate;
    if (RotationRateToUse <= 0.f) // Fallback to base class TurnSpeed if SpecificAITurnRate is not set or invalid
    {
        RotationRateToUse = TurnSpeed; // Assumes GetTurnSpeed() is accessible and returns degrees/second
    }

    // Interpolate the current rotation towards the desired rotation at a constant rate.
    FRotator NewRotation = FMath::RInterpConstantTo(CurrentActorRotation, DesiredRotation, DeltaTime, RotationRateToUse);

    // Set the new actor rotation.
    // SetActorRotation directly manipulates the transform.
    // If the root component is simulating physics, this can sometimes be fought by the physics engine.
    // Using ETeleportType::TeleportPhysics can help by resetting the physics state to the new transform.
    // However, for simple yaw on a constrained body, direct SetActorRotation might be sufficient.
    // Let's try without TeleportPhysics first.
    SetActorRotation(NewRotation);

    // Optional Log
    /*
    UE_LOG(LogSolaraqAI, Log, TEXT("%s TurnTowards (Transform): CurrentYaw: %.1f, TargetYaw: %.1f, NewYaw: %.1f, Rate: %.1f Dt: %.3f"),
           *GetName(), CurrentActorRotation.Yaw, DesiredRotation.Yaw, NewRotation.Yaw, RotationRateToUse, DeltaTime);
    */
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

     //UE_LOG(LogSolaraqAI, Warning, TEXT("%s AI RequestMoveForward: %.2f"), *GetName(), Value);
}