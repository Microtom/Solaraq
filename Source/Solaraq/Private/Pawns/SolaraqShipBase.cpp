// SolaraqShipBase.cpp

#include "Pawns/SolaraqShipBase.h"

#include "AIController.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/EngineTypes.h"
#include "GenericTeamAgentInterface.h"
#include "Components/SphereComponent.h"
#include "GameFramework/DamageType.h"
#include "Engine/CollisionProfile.h"      // For using standard collision profile names
#include "Logging/SolaraqLogChannels.h"   // Use our custom logger!
#include "GameFramework/PlayerController.h" // Needed for IsLocalController() potentially later
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Projectiles/SolaraqProjectile.h"

// Simple Logging Helper Macro
#define NET_LOG(LogCat, Verbosity, Format, ...) \
UE_LOG(LogCat, Verbosity, TEXT("[%s] %s: " Format), \
(GetNetMode() == NM_Client ? TEXT("CLIENT") : (GetNetMode() == NM_ListenServer || GetNetMode() == NM_DedicatedServer ? TEXT("SERVER") : TEXT("STANDALONE"))), \
*FString(__FUNCTION__), \
##__VA_ARGS__)



// Sets default values
ASolaraqShipBase::ASolaraqShipBase()
{
    // Set this pawn to call Tick() every frame. You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true; // Enable ticking

    // Initialize Health (can also be done in BeginPlay if needed)
    CurrentHealth = MaxHealth;
    bIsDead = false; // Ensure dead state is false initially
    
    // Initialize variables here if not using UPROPERTY defaults or need complex init
    ThrustForce = 1400000.0f; // Can also set defaults here
    TurnSpeed = 110.0f;
    NormalMaxSpeed = 2300.0f;
    Dampening = 0.05f;

    CurrentEnergy = MaxEnergy; // Start full
    bIsBoosting = false;
    bIsAttemptingBoostInput = false;
    LastBoostStopTime = -1.0f; // Initialize regen timer state
    
    // --- Create and Setup Collision and Physics Root ---
    CollisionAndPhysicsRoot = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionAndPhysicsRoot"));
    SetRootComponent(CollisionAndPhysicsRoot); // <<<--- Set Box as the actual root
    CollisionAndPhysicsRoot->SetMobility(EComponentMobility::Movable);
    CollisionAndPhysicsRoot->InitSphereRadius(40.f);
    //CollisionAndPhysicsRoot->InitBoxExtent(FVector(50.0f, 50.0f, 25.0f));

    // --- Enable Physics on the Root Box ---
    CollisionAndPhysicsRoot->SetSimulatePhysics(true); // <<<--- Physics on the primitive root
    CollisionAndPhysicsRoot->SetEnableGravity(false);
    CollisionAndPhysicsRoot->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName); // Collision on the root
    CollisionAndPhysicsRoot->SetNotifyRigidBodyCollision(true); // For hit events

    // --- Apply Constraints and Damping to the Root Box's Physics Body ---
    if (FBodyInstance* BodyInst = CollisionAndPhysicsRoot->GetBodyInstance()) // Get BodyInstance from the root Box
    {
        // Lock unwanted movement/rotation
        BodyInst->bLockZTranslation = true;
        BodyInst->bLockXRotation = true; // Lock Roll
        BodyInst->bLockYRotation = true; // Lock Pitch
        BodyInst->bLockZRotation = false; // Allow Yaw

        // Add damping
        BodyInst->LinearDamping = Dampening;
        BodyInst->AngularDamping = 0.8f;

        UE_LOG(LogSolaraqSystem, Log, TEXT("Physics constraints and damping set for CollisionAndPhysicsRoot on %s"), *GetNameSafe(this));
    }
    else
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("Could not get BodyInstance for CollisionAndPhysicsRoot on %s"), *GetNameSafe(this));
    }
    
    // --- Create and Setup Ship Mesh Component ---
    ShipMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
    // Set the mesh as the root component. This is important for physics simulation.
    // APawn's default root might be a sphere or capsule, which isn't ideal for a ship's physics root.
    ShipMeshComponent->SetupAttachment(RootComponent);
    
    // Enable physics simulation for the mesh. This is key for Subspace/SC style movement.
    ShipMeshComponent->SetSimulatePhysics(false);
    ShipMeshComponent->SetEnableGravity(false); // No gravity in space!
    // Set a default collision profile. "Pawn" is often suitable, or create a custom one later.
    ShipMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
    // Make sure it generates hit events if we need them later for collisions
    ShipMeshComponent->SetNotifyRigidBodyCollision(true);

    // --- Create Muzzle Point Component ---
    MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
    if (MuzzlePoint && ShipMeshComponent) // Attach to mesh if available
    {
        MuzzlePoint->SetupAttachment(ShipMeshComponent);
        // Optional: Set a default relative location slightly in front of where the mesh center might be
        MuzzlePoint->SetRelativeLocation(FVector(100.0f, 0.0f, 0.0f)); // Adjust X offset as needed
    }
    else if (MuzzlePoint && CollisionAndPhysicsRoot) // Fallback to physics root
    {
        MuzzlePoint->SetupAttachment(CollisionAndPhysicsRoot);
        MuzzlePoint->SetRelativeLocation(FVector(100.0f, 0.0f, 0.0f));
    }

    // Default Weapon Values
    ProjectileMuzzleSpeed = 8000.0f;
    FireRate = 0.5f;
    LastFireTime = -1.0f;
    
    // --- Create and Setup Spring Arm Component ---
    SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    // Attach the spring arm to the root (our ShipMeshComponent)
    SpringArmComponent->SetupAttachment(RootComponent);
    // Set default rotation - pointing straight down for a top-down view
    SpringArmComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
    // Set default arm length (distance from the ship)
    SpringArmComponent->TargetArmLength = 3000.0f; // Adjust this value later
    // Disable camera lag and collision checks for now, enable if needed
    SpringArmComponent->bEnableCameraLag = false;
    SpringArmComponent->bEnableCameraRotationLag = false;
    SpringArmComponent->bDoCollisionTest = false; // Avoid camera bumping off things for now

    // --- Prevent camera rotation when ship turns --- <<<< ADD THESE LINES
    SpringArmComponent->bInheritPitch = false;
    SpringArmComponent->bInheritYaw = false;
    SpringArmComponent->bInheritRoll = false;

    // --- Replication Settings ---
    bReplicates = true; // Enable Actor replication for this Pawn class
    SetReplicateMovement(true); // Use built-in Actor transform replication
    CollisionAndPhysicsRoot->SetIsReplicated(true);
    
    // Allow the Pawn to be controlled by the default PlayerController
    // AutoPossessPlayer = EAutoReceiveInput::Player0; // We can set this here, but often better set in GameMode or World Settings

    
     // Log that the base ship was constructed
    UE_LOG(LogSolaraqGeneral, Log, TEXT("ASolaraqShipBase %s Constructed"), *GetName());
}

void ASolaraqShipBase::Client_SetVisualScale_Implementation(float NewScaleFactor)
{
    // Apply the scale locally on the client
    ApplyVisualScale(NewScaleFactor);
}

void ASolaraqShipBase::Client_ResetVisualScale_Implementation()
{
    // Apply the default scale locally on the client
    ApplyVisualScale(1.0f); // Assuming 1.0 represents the default scale factor
}

void ASolaraqShipBase::Server_DockWithPad(UDockingPadComponent* PadToDockWith)
{
    // --- SERVER ONLY ---
    if (!HasAuthority() || !IsValid(PadToDockWith) || bIsDocked) // Only dock if not already docked
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("Ship %s: Dock attempt rejected. Authority: %d, ValidPad: %d, IsDocked: %d"),
            *GetName(), HasAuthority(), IsValid(PadToDockWith), bIsDocked);
        return;
    }

    UE_LOG(LogSolaraqSystem, Log, TEXT("Ship %s: Server_DockWithPad called by %s."), *GetName(), *PadToDockWith->GetName());

    bIsDocked = true;
    DockedToPadComponent = PadToDockWith;

    // Perform the physics changes and attachment
    PerformDockingAttachment();
    DisableSystemsForDocking();

    // Force replication now if needed (usually happens automatically)
    // ForceNetUpdate();

    // Notify clients via OnRep functions
    OnRep_IsDocked(); // Call locally on server too for consistency if needed
    OnRep_DockedToPad();
}

void ASolaraqShipBase::Server_RequestUndock_Implementation()
{
}

void ASolaraqShipBase::OnRep_IsDocked()
{
    // Called on CLIENTS when bIsDocked changes value due to replication
    if (bIsDocked)
    {
        UE_LOG(LogSolaraqSystem, Verbose, TEXT("CLIENT Ship %s: Received docked state=true."), *GetName());
        // Client-side cosmetic changes:
        // - Maybe hide thruster FX?
        // - Update HUD to show "Docked" status
        // - Disable input locally if using Option 3 above
        // Note: Attachment is handled by engine's movement replication, physics disabling isn't replicated directly
        // but the lack of simulation + attachment achieves the same result visually.

        // Crucially, make sure the client *stops* simulating physics if it was doing any client-side prediction
        if(CollisionAndPhysicsRoot && CollisionAndPhysicsRoot->IsSimulatingPhysics())
        {
            CollisionAndPhysicsRoot->SetSimulatePhysics(false);
            UE_LOG(LogSolaraqSystem, Verbose, TEXT("CLIENT Ship %s: Disabling local physics simulation due to docking."), *GetName());
        }
    }
    else // Undocked
    {
        UE_LOG(LogSolaraqSystem, Verbose, TEXT("CLIENT Ship %s: Received docked state=false."), *GetName());
        // Client-side cosmetic changes:
        // - Restore thruster FX?
        // - Update HUD
        // - Re-enable input locally if using Option 3
        // Ensure physics simulation is re-enabled visually (though server dictates actual position)
        // Note: Detachment is handled by engine replication. Client may need to re-enable physics
        // simulation locally IF it uses it for prediction/smoothing, but be careful not to fight the server.
        if(CollisionAndPhysicsRoot && !CollisionAndPhysicsRoot->IsSimulatingPhysics())
        {
            CollisionAndPhysicsRoot->SetSimulatePhysics(true); // Re-enable local simulation if needed for effects/prediction
            UE_LOG(LogSolaraqSystem, Verbose, TEXT("CLIENT Ship %s: Re-enabling local physics simulation due to undocking."), *GetName());
        }
    }
}

void ASolaraqShipBase::OnRep_DockedToPad()
{
    // Called on CLIENTS when DockedToPadComponent changes value
    UE_LOG(LogSolaraqSystem, Verbose, TEXT("CLIENT Ship %s: Received docked pad reference: %s."), *GetName(), *GetNameSafe(DockedToPadComponent));
    // Update UI with station name, etc.
}

bool ASolaraqShipBase::IsDockedTo(const UDockingPadComponent* Pad) const
{
    return bIsDocked && (DockedToPadComponent == Pad);
}

float ASolaraqShipBase::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
    class AController* EventInstigator, AActor* DamageCauser)
{
    const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    // Don't process damage if already dead or no actual damage
    if (bIsDead || ActualDamage <= 0.0f)
    {
        return 0.0f;
    }

    // Only the server modifies health authoritatively
    if (HasAuthority())
    {
        CurrentHealth = FMath::Clamp(CurrentHealth - ActualDamage, 0.0f, MaxHealth);
        NET_LOG(LogSolaraqCombat, Log, TEXT("Took %.1f damage from %s. CurrentHealth: %.1f"), ActualDamage, *GetNameSafe(DamageCauser), CurrentHealth);

        // Check if the ship is destroyed
        if (CurrentHealth <= 0.0f)
        {
            HandleDestruction();
        }
        // Note: CurrentHealth replication happens automatically due to the ReplicatedUsing flag.
    }
    // Optionally, non-authoritative clients could play cosmetic impact effects here immediately

    return ActualDamage; // Return the damage that was actually applied
}

float ASolaraqShipBase::GetHealthPercentage() const
{
    if (MaxHealth <= 0.0f)
    {
        return 0.0f; // Avoid division by zero
    }
    return CurrentHealth / MaxHealth;
}

// Called when the game starts or when spawned
void ASolaraqShipBase::BeginPlay()
{
    Super::BeginPlay();

    // Ensure energy starts at max on server and client
    CurrentEnergy = MaxEnergy;

    // Cache the initial scale of the visual mesh component
    if (ShipMeshComponent)
    {
        DefaultVisualMeshScale = ShipMeshComponent->GetRelativeScale3D();
        // Ensure LastAppliedScaleFactor starts correct too
        LastAppliedScaleFactor = FMath::Max(DefaultVisualMeshScale.X, FMath::Max(DefaultVisualMeshScale.Y, DefaultVisualMeshScale.Z)); // Approx for non-uniform
        if (DefaultVisualMeshScale.IsUniform())
        {
            LastAppliedScaleFactor = DefaultVisualMeshScale.X;
        } else {
            UE_LOG(LogSolaraqCelestials, Warning, TEXT("Ship %s has non-uniform default scale. Scale factor RPC might be slightly inaccurate."), *GetName());
            // Default to 1.0f if non-uniform to be safe, or handle non-uniform scaling explicitly
            LastAppliedScaleFactor = 1.0f;
            DefaultVisualMeshScale = FVector::OneVector; // Reset to uniform for calculations
        }
    }

    // Good place to ensure health is set correctly, especially if MaxHealth could change
    if (HasAuthority()) // Only server needs to initialize authoritatively
    {
        CurrentHealth = MaxHealth;
        bIsDead = false; // Double-check state
    }

    // Reset energy (existing code)
    CurrentEnergy = MaxEnergy;
    
    UE_LOG(LogSolaraqGeneral, Log, TEXT("ASolaraqShipBase %s BeginPlay called."), *GetName());
}

void ASolaraqShipBase::ApplyVisualScale(float ScaleFactor)
{
    // Optional: Optimization - Only apply if the scale factor has actually changed significantly
    if (!FMath::IsNearlyEqual(ScaleFactor, LastAppliedScaleFactor, 0.01f)) // Check within 1% tolerance
    {
        if (ShipMeshComponent)
        {
            // Apply uniformly based on the original DefaultVisualMeshScale
            ShipMeshComponent->SetRelativeScale3D(DefaultVisualMeshScale * ScaleFactor);
            LastAppliedScaleFactor = ScaleFactor; // Update last applied factor
            //UE_LOG(LogSolaraqMovement, VeryVerbose, TEXT("CLIENT: Applied visual scale %.3f"), ScaleFactor);
        }
    }
}

// --- Input Processing ---

// Called by client via input binding -> sends request to server
void ASolaraqShipBase::Server_SetAttemptingBoost_Implementation(bool bAttempting)
{
    // This runs ON THE SERVER
    bIsAttemptingBoostInput = bAttempting;
    // Server Tick will now use this value to potentially set bIsBoosting
}

// --- Server-Side Movement Logic ---
void ASolaraqShipBase::ProcessMoveForwardInput(float Value)
{
    // Only the server should execute the actual physics change
    if (HasAuthority())
    {
        if (CollisionAndPhysicsRoot && FMath::Abs(Value) > KINDA_SMALL_NUMBER) // Check Value is not zero
        {
            // Apply boost multiplier if boosting
            const float ActualThrust = bIsBoosting ? (ThrustForce * BoostThrustMultiplier) : ThrustForce;
            
            const FVector ForceDirection = GetActorForwardVector();
            const FVector ForceToAdd = ForceDirection * Value * ActualThrust; // Value already incorporates direction (-1 or 1)

            // Using AddForce with bAccelChange=false (default) considers mass
            CollisionAndPhysicsRoot->AddForce(ForceToAdd, NAME_None, false);

            // Log for debugging
            // UE_LOG(LogSolaraqMovement, Verbose, TEXT("[%s] Applying Forward Force: %.2f"), HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"), ForceToAdd.Size());
        }
    }
    // Note: No client-side prediction implemented here yet
}

void ASolaraqShipBase::ProcessTurnInput(float Value)
{
    // Only the server should execute the actual rotation change
    if (HasAuthority())
    {
        if (FMath::Abs(Value) > KINDA_SMALL_NUMBER) // Check Value is not zero
        {
            const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
            const float RotationThisFrame = Value * TurnSpeed * DeltaSeconds; // Value has direction (-1 or 1)

            // Add rotation directly to the Actor
            AddActorLocalRotation(FRotator(0.0f, RotationThisFrame, 0.0f));

            // Log for debugging
            // UE_LOG(LogSolaraqMovement, Verbose, TEXT("[%s] Applying Turn Rotation: %.2f degrees"), HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"), RotationThisFrame);
        }
    }
    // Note: No client-side prediction implemented here yet
}

// --- Replication Functions ---
void ASolaraqShipBase::Server_SendMoveForwardInput_Implementation(float Value)
{
    // This function now executes ON THE SERVER, called by a client
    ProcessMoveForwardInput(Value);
}

void ASolaraqShipBase::Server_SendTurnInput_Implementation(float Value)
{
    // This function executes ON THE SERVER, called by a client
    ProcessTurnInput(Value);

    // Server updates the state variable that gets replicated
    SetTurnInputForRoll(Value);
}

// Called every frame
void ASolaraqShipBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // --- Visual Roll Logic (Runs on Server and Clients) ---
    if (ShipMeshComponent)
    {
        // TargetRollAngle uses the potentially replicated CurrentTurnInputForRoll value
        const float TargetRollAngle = CurrentTurnInputForRoll * MaxTurnRollAngle;
        // CurrentVisualRoll is interpolated locally
        CurrentVisualRoll = FMath::FInterpTo(CurrentVisualRoll, TargetRollAngle, DeltaTime, RollInterpolationSpeed);
        // Apply locally calculated roll
        FRotator CurrentMeshRelativeRotation = ShipMeshComponent->GetRelativeRotation();
        ShipMeshComponent->SetRelativeRotation(FRotator(
            CurrentMeshRelativeRotation.Pitch,
            CurrentMeshRelativeRotation.Yaw,
            CurrentVisualRoll // Apply interpolated Roll
        ));
    }
    
    // Server handles authoritative state changes and energy management
    if (HasAuthority())
    {
        const UWorld* World = GetWorld();
        const float CurrentTime = World ? World->GetTimeSeconds() : 0.f;

        //NET_LOG(LogSolaraqMovement, Warning, TEXT("Tick Start - AttemptBoostInput: %d, CurrentEnergy: %.2f, IsBoosting: %d, LastBoostStopTime: %.2f"), bIsAttemptingBoostInput, CurrentEnergy, bIsBoosting, LastBoostStopTime);
        
        // Determine if we SHOULD be boosting based on input and energy
        bool bCanBoost = bIsAttemptingBoostInput && CurrentEnergy > 0;

        // Update actual boost state
        if (bCanBoost != bIsBoosting)
        {
            bIsBoosting = bCanBoost;
            if (!bIsBoosting) // If we just stopped boosting
            {
                LastBoostStopTime = CurrentTime;
                UE_LOG(LogSolaraqMovement, Verbose, TEXT("Stopped Boosting. LastBoostStopTime: %.2f"), LastBoostStopTime);
            }
            else // If we just started boosting
            {
                LastBoostStopTime = -1.f; // Reset regen timer
                UE_LOG(LogSolaraqMovement, Verbose, TEXT("Started Boosting."));
            }
            // Note: bIsBoosting replication will trigger OnRep_IsBoosting on clients
        }

        // Drain or Regen Energy
        if (bIsBoosting)
        {
            CurrentEnergy -= EnergyDrainRate * DeltaTime;
            CurrentEnergy = FMath::Max(0.f, CurrentEnergy); // Clamp Min
            if (CurrentEnergy == 0.f)
            {
                UE_LOG(LogSolaraqMovement, Log, TEXT("Boost energy depleted."));
                // Force stop boosting if energy runs out (this will be handled next tick by bCanBoost logic)
            }
        }
        else // Not Boosting
        {
            // Check if regen delay has passed
            if (LastBoostStopTime > 0.f && CurrentTime >= LastBoostStopTime + EnergyRegenDelay)
            {
                if(CurrentEnergy < MaxEnergy)
                {
                    CurrentEnergy += EnergyRegenRate * DeltaTime;
                    CurrentEnergy = FMath::Min(MaxEnergy, CurrentEnergy); // Clamp Max
                }
                else // Energy is full, reset timer so we don't check every frame
                {
                    LastBoostStopTime = -1.f;
                }
            }
        }
        // Note: CurrentEnergy replication will trigger OnRep_CurrentEnergy on clients

        
        // --- Velocity Clamping (Server-Side) ---
        // Only the server modifies the authoritative velocity directly like this.
        // The result is replicated to clients via bReplicateMovement.
    
        if (CollisionAndPhysicsRoot && NormalMaxSpeed > 0.0f)
        {
            // Get current velocity
            FVector CurrentVelocity = CollisionAndPhysicsRoot->GetPhysicsLinearVelocity();

            // Calculate current speed
            float CurrentSpeed = CurrentVelocity.Size();

            // Check if exceeding max speed
            if (CurrentSpeed > NormalMaxSpeed)
            {
                // Calculate velocity direction and clamp its magnitude to MaxSpeed
                FVector ClampedVelocity = CurrentVelocity.GetSafeNormal() * NormalMaxSpeed;

                // Set the physics velocity directly
                CollisionAndPhysicsRoot->SetPhysicsLinearVelocity(ClampedVelocity);

                // Log for debugging (optional)
                // UE_LOG(LogSolaraqMovement, Verbose, TEXT("Velocity Clamped from %.2f to %.2f"), CurrentSpeed, MaxSpeed);
            }
        }
    }

    // Clamp velocity - Can run on all machines for smoother visuals,
    // but server's physics state is the authority.
    ClampVelocity();
}

void ASolaraqShipBase::ClampVelocity()
{
    if (!CollisionAndPhysicsRoot || !CollisionAndPhysicsRoot->IsSimulatingPhysics())
    {
        return; // No physics root to clamp
    }

    const float CurrentMaxSpeed = bIsBoosting ? BoostMaxSpeed : NormalMaxSpeed;
    //UE_LOG(LogSolaraqMovement, Warning, TEXT("[%s] CurrentMaxSpeed: %.2f, BoostMaxSpeed: %.2f, NormalMaxSpeed: %.1f, IsBoosting: %d"), HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"), CurrentMaxSpeed, BoostMaxSpeed, NormalMaxSpeed, bIsBoosting);
    
    const float MaxSpeedSq = FMath::Square(CurrentMaxSpeed);
    
    const FVector CurrentVelocity = CollisionAndPhysicsRoot->GetPhysicsLinearVelocity();
    const float CurrentSpeedSq = CurrentVelocity.SizeSquared();
    //UE_LOG(LogSolaraqMovement, Warning, TEXT("[%s]  CurrentMaxSpeedSq: %1f, MaxSpeedSq: %0.1f"), HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"), CurrentSpeedSq, MaxSpeedSq);
    
    if (CurrentSpeedSq > MaxSpeedSq)
    {
        const FVector ClampedVelocity = CurrentVelocity.GetSafeNormal() * CurrentMaxSpeed;
        CollisionAndPhysicsRoot->SetPhysicsLinearVelocity(ClampedVelocity);
        //UE_LOG(LogSolaraqMovement, Warning, TEXT("[%s] CurrentVelocity: %.1f, CurrentMaxSpeed: %.2f, BoostMaxSpeed: %.2f, NormalMaxSpeed: %.1f, IsBoosting: %d"), HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"), CurrentVelocity.Size(), CurrentMaxSpeed, BoostMaxSpeed, NormalMaxSpeed, bIsBoosting);
    
        //UE_LOG(LogSolaraqMovement, Warning, TEXT("[%s] Clamping speed. Current: %.2f, Max: %.2f"), HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"), CurrentVelocity.Size(), CurrentMaxSpeed);
    }
}

void ASolaraqShipBase::PerformDockingAttachment()
{
    // --- SERVER ONLY ---
    if (!HasAuthority() || !IsValid(DockedToPadComponent) || !CollisionAndPhysicsRoot) return;

    AActor* StationActor = DockedToPadComponent->GetOwner();
    if (!IsValid(StationActor)) return;

    // 1. Disable Physics
    CollisionAndPhysicsRoot->SetSimulatePhysics(false);
    CollisionAndPhysicsRoot->SetPhysicsLinearVelocity(FVector::ZeroVector); // Stop residual movement
    CollisionAndPhysicsRoot->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

    // 2. Determine Attach Target
    USceneComponent* TargetAttachComp = DockedToPadComponent->GetAttachPoint();
    if (!IsValid(TargetAttachComp))
    {
        // Fallback to station's root component if specific attach point isn't set
        TargetAttachComp = StationActor->GetRootComponent();
        UE_LOG(LogSolaraqSystem, Warning, TEXT("Ship %s docking: No specific AttachPoint set on pad %s. Attaching to station root %s."),
            *GetName(), *DockedToPadComponent->GetName(), *GetNameSafe(TargetAttachComp));
    }

    if (!IsValid(TargetAttachComp)) // Still no valid target? Abort.
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("Ship %s docking: Cannot find a valid component to attach to on station %s!"), *GetName(), *StationActor->GetName());
        // Consider rolling back the docking state here?
        return;
    }

    // 3. Attach
    // Use KeepWorldTransform first to avoid instant jump, then potentially smooth align
    FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, false); // Keep world, don't weld simulated bodies
    GetRootComponent()->AttachToComponent(TargetAttachComp, AttachRules);

    UE_LOG(LogSolaraqSystem, Log, TEXT("Ship %s: Attached to %s on %s."), *GetName(), *TargetAttachComp->GetName(), *StationActor->GetName());

    // TODO Optional: Implement smooth alignment to a specific relative offset/rotation
    // This could use a Timer, Timeline, or Latent Action to lerp RelativeLocation/Rotation
    // to FVector::ZeroVector / FRotator::ZeroRotator over a short duration AFTER attaching.
}

void ASolaraqShipBase::PerformUndockingDetachment()
{
    // --- SERVER ONLY ---
    if (!HasAuthority() || !CollisionAndPhysicsRoot) return;

    // 1. Detach
    FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true); // Keep world, call Modify() on physics state
    GetRootComponent()->DetachFromComponent(DetachRules);

    UE_LOG(LogSolaraqSystem, Log, TEXT("Ship %s: Detached."), *GetName());

    // 2. Re-enable Physics (AFTER detaching)
    CollisionAndPhysicsRoot->SetSimulatePhysics(true);

    // Optional: Apply a small impulse to push away from the station
    // const FVector ImpulseDirection = GetActorForwardVector() * -1.0f; // Push backwards
    // CollisionAndPhysicsRoot->AddImpulse(ImpulseDirection * 10000.0f, NAME_None, true); // Adjust magnitude, true=VelocityChange
}

void ASolaraqShipBase::DisableSystemsForDocking()
{
    // --- SERVER ONLY --- (Called by Server_DockWithPad)

    // Disable Player Input for Movement/Firing
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC && PC->IsLocalController()) // Check if it's the actual player controller
    {
        // Option 1: Direct Disable (might disable menus too, be careful)
        // DisableInput(PC);

        // Option 2: Modify input handling logic (preferred)
        // Inside your input handling functions (ProcessMoveForwardInput, ProcessTurnInput, Fire Weapon etc.):
        // Add check: if (IsDocked()) { return; }
        UE_LOG(LogSolaraqSystem, Log, TEXT("Ship %s: Movement/Weapon input should now be ignored due to docked state."), *GetName());

        // Option 3: Tell the Controller (requires custom PlayerController class)
        // ASolaraqPlayerController* MyPC = Cast<ASolaraqPlayerController>(PC);
        // if(MyPC) { MyPC->SetShipControlsEnabled(false); }
    }

    // Stop boosting immediately if applicable
    bIsAttemptingBoostInput = false;
    bIsBoosting = false; // Ensure boost state is off

    // Other systems? Stop weapon charging, cancel scans, etc.
}

void ASolaraqShipBase::EnableSystemsAfterUndocking()
{
    // --- SERVER ONLY --- (Called by Server_RequestUndock)

    // Re-enable Player Input
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC && PC->IsLocalController())
    {
        // Option 1:
        // EnableInput(PC);

        // Option 2: Input functions will stop returning early because IsDocked() is now false.

        // Option 3:
        // ASolaraqPlayerController* MyPC = Cast<ASolaraqPlayerController>(PC);
        // if(MyPC) { MyPC->SetShipControlsEnabled(true); }
        UE_LOG(LogSolaraqSystem, Log, TEXT("Ship %s: Movement/Weapon input should now be processed again."), *GetName());
    }
    // Re-allow boosting etc.
}

// Called to bind functionality to input
void ASolaraqShipBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Base class doesn't handle specific player inputs (like Thrust, Fire).
    // The derived player ship Blueprint (or C++ class) will handle this.
    // We *could* bind shared inputs here (e.g., maybe a 'scan' function common to all ships?)
    UE_LOG(LogSolaraqGeneral, Verbose, TEXT("ASolaraqShipBase %s SetupPlayerInputComponent called."), *GetName());
}

FGenericTeamId ASolaraqShipBase::GetGenericTeamId() const
{
    // If player possesses this directly, return the player's team ID
    return TeamId;
}

ETeamAttitude::Type ASolaraqShipBase::GetTeamAttitudeTowards(const AActor& Other) const
{
    // If this pawn IS the player pawn:
    if (const APawn* OtherPawn = Cast<const APawn>(&Other))
    {
        // Check the Other actor directly first
        const IGenericTeamAgentInterface* OtherTeamAgentPawn = Cast<const IGenericTeamAgentInterface>(&Other);
        if(OtherTeamAgentPawn)
        {
            if (OtherTeamAgentPawn->GetGenericTeamId() == GetGenericTeamId())
            {
                return ETeamAttitude::Friendly;
            }
            else if (OtherTeamAgentPawn->GetGenericTeamId().GetId() != FGenericTeamId::NoTeam.GetId())
            {
                return ETeamAttitude::Hostile; // Different valid team = Hostile
            }
        }

        // If pawn check failed, check the Other actor's CONTROLLER
        if (const IGenericTeamAgentInterface* OtherTeamAgentController = Cast<const IGenericTeamAgentInterface>(OtherPawn->GetController()))
        {
            if (OtherTeamAgentController->GetGenericTeamId() == GetGenericTeamId())
            {
                return ETeamAttitude::Friendly;
            }
            else if (OtherTeamAgentController->GetGenericTeamId().GetId() != FGenericTeamId::NoTeam.GetId())
            {
                return ETeamAttitude::Hostile; // Different valid team = Hostile
            }
        }

        // Fallback: If the other pawn is controlled by an AI Controller (and not handled above), treat as hostile?
        if (Cast<AAIController>(OtherPawn->GetController()))
        {
            return ETeamAttitude::Hostile;
        }
    }
    // Default attitude
    return ETeamAttitude::Neutral;
}

void ASolaraqShipBase::OnRep_TurnInputForRoll()
{
    // Called on Clients when CurrentTurnInputForRoll changes via replication.
    // The Tick function will pick up the change and start interpolating.
    // Usually no specific code needed here unless debugging or needing instant visual snap.
    //UE_LOG(LogSolaraqVisuals, VeryVerbose, TEXT("CLIENT %s OnRep_TurnInputForRoll: Received %.2f"), *GetName(), CurrentTurnInputForRoll);
}

void ASolaraqShipBase::SetTurnInputForRoll(float TurnValue)
{
    // Clamp the input value
    float ClampedValue = FMath::Clamp(TurnValue, -1.0f, 1.0f);

    // Optional: Only set on server if value actually changes, reduces replication noise slightly
    // Needs authority check because client prediction might call this too.
    if (HasAuthority())
    {
        if(CurrentTurnInputForRoll != ClampedValue) // Check if changed
        {
            CurrentTurnInputForRoll = ClampedValue;
            // Server's local value is set, replication system will handle sending if changed.
        }
    }
    else if (IsLocallyControlled()) // Allow local player to set for immediate visual feedback
    {
        // We don't directly set CurrentTurnInputForRoll here on the client,
        // as the server value will replicate down. But the PlayerController
        // already has the input value, so the Tick function can use it immediately
        // if needed for prediction (though our current Tick uses the replicated value).
        // For simplicity with this method, we let the server state drive visuals.
    }
    // Note: AI Controller calls this only on the Server, setting the authoritative value.
}

void ASolaraqShipBase::PerformFireWeapon()
{
    // --- Server-Side Execution Only ---
    if (!HasAuthority()) return;
    if (IsDead()) return;

    // --- Check Cooldown ---
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f; // Safely get time
    if (CurrentTime < LastFireTime + FireRate)
    {
        // UE_LOG(LogSolaraqCombat, VeryVerbose, TEXT("%s PerformFireWeapon: Cooldown Active"), *GetName());
        return;
    }

    // --- Check Required Assets & Components ---
    if (!ProjectileClass)
    {
        UE_LOG(LogSolaraqCombat, Error, TEXT("%s PerformFireWeapon: ProjectileClass is NULL! Assign in derived Blueprint defaults."), *GetName());
        return;
    }
    if (!MuzzlePoint)
    {
        UE_LOG(LogSolaraqCombat, Error, TEXT("%s PerformFireWeapon: MuzzlePoint component is NULL!"), *GetName());
        return;
    }
    UWorld* const World = GetWorld();
    if (!World)
    {
        UE_LOG(LogSolaraqCombat, Error, TEXT("%s PerformFireWeapon: GetWorld() returned NULL!"), *GetName());
        return;
    }

    // --- Calculate Spawn Transform & Velocity ---
    const FVector MuzzleLocation = MuzzlePoint->GetComponentLocation();
    const FRotator MuzzleRotation = MuzzlePoint->GetComponentRotation();
    // Combine ship velocity with muzzle velocity
    const FVector ShipVelocity = CollisionAndPhysicsRoot ? CollisionAndPhysicsRoot->GetPhysicsLinearVelocity() : FVector::ZeroVector;
    const FVector MuzzleVelocity = MuzzleRotation.Vector() * ProjectileMuzzleSpeed; // Direction * Speed
    const FVector FinalVelocity = ShipVelocity + MuzzleVelocity;

    UE_LOG(LogSolaraqCombat, Warning, TEXT("%s PerformFireWeapon: Spawning %s. MuzzleLoc:%s Rot:%s ShipVel:%s MuzzleVel:%s FinalVel:%s"),
        *GetName(), *ProjectileClass->GetName(), *MuzzleLocation.ToString(), *MuzzleRotation.ToString(),
        *ShipVelocity.ToString(), *MuzzleVelocity.ToString(), *FinalVelocity.ToString());

    // --- Set Spawn Parameters ---
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // --- Spawn the Projectile ---
    AActor* SpawnedProjectile = World->SpawnActor<ASolaraqProjectile>(ProjectileClass, MuzzleLocation, MuzzleRotation, SpawnParams);

    // --- Set Velocity & Cooldown ---
    if (SpawnedProjectile)
    {
        // Find the projectile movement component on the spawned actor
        UProjectileMovementComponent* ProjMoveComp = SpawnedProjectile->FindComponentByClass<UProjectileMovementComponent>();
        if (ProjMoveComp)
        {
            ProjMoveComp->Velocity = FinalVelocity; // Set the calculated velocity
            ProjMoveComp->UpdateComponentVelocity(); // Ensure component registers the change immediately
            UE_LOG(LogSolaraqProjectile, Warning, TEXT("%s PerformFireWeapon: Spawned %s, Set Velocity to %s"),
                *GetName(), *SpawnedProjectile->GetName(), *FinalVelocity.ToString());

            LastFireTime = CurrentTime; // Reset cooldown ONLY if spawn and velocity set were successful
        }
        else
        {
            UE_LOG(LogSolaraqProjectile, Warning, TEXT("%s PerformFireWeapon: Spawned %s but it has NO ProjectileMovementComponent! Cannot set velocity."),
                *GetName(), *SpawnedProjectile->GetName());
            // SpawnedProjectile->Destroy(); // Optionally destroy the invalid projectile
        }
    }
    else
    {
         UE_LOG(LogSolaraqProjectile, Error, TEXT("%s PerformFireWeapon: World->SpawnActor failed for %s!"), *GetName(), *ProjectileClass->GetName());
    }
}

void ASolaraqShipBase::Multicast_PlayDestructionEffects_Implementation()
{
    // This runs on the Server AND all Clients

    // --- Add your visual and audio effects here ---

    // Example: Spawn a particle effect at the ship's location
    // UParticleSystem* ExplosionEffect = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/Path/To/Your/P_Explosion.P_Explosion")); // Load your effect
    // if (ExplosionEffect)
    // {
    //     UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionEffect, GetActorLocation(), GetActorRotation());
    // }

    // Example: Play a sound effect
    // USoundCue* ExplosionSound = LoadObject<USoundCue>(nullptr, TEXT("/Game/Path/To/Your/S_Explosion_Cue.S_Explosion_Cue")); // Load your sound
    // if (ExplosionSound)
    // {
    //     UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation());
    // }

    // Example: Hide the mesh component (can also be done in OnRep_IsDead for certainty)
    if (ShipMeshComponent)
    {
        ShipMeshComponent->SetVisibility(false, true); // Hide mesh and child components
        ShipMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Ensure collision off
    }

    NET_LOG(LogSolaraqCombat, Verbose, TEXT("Multicast_PlayDestructionEffects executed on %s"), *GetName());
}

void ASolaraqShipBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Replicate these variables
    DOREPLIFETIME(ASolaraqShipBase, CurrentEnergy);
    DOREPLIFETIME(ASolaraqShipBase, bIsAttemptingBoostInput); // Replicate the *request*
    DOREPLIFETIME(ASolaraqShipBase, bIsBoosting);            // Replicate the *actual* state
    DOREPLIFETIME_CONDITION(ASolaraqShipBase, MaxEnergy, COND_InitialOnly); // Max energy likely won't change after spawn
    DOREPLIFETIME(ASolaraqShipBase, bIsDocked);
    DOREPLIFETIME(ASolaraqShipBase, DockedToPadComponent);
    // Replicate Health and Death State
    DOREPLIFETIME(ASolaraqShipBase, CurrentHealth);
    DOREPLIFETIME(ASolaraqShipBase, bIsDead);
    // Replicate the turn input value
    DOREPLIFETIME(ASolaraqShipBase, CurrentTurnInputForRoll); 
}

// --- Replication Notifiers ---

void ASolaraqShipBase::OnRep_CurrentEnergy()
{
    // Called automatically on clients when CurrentEnergy is updated by the server.
    // Use this to update UI elements binded to CurrentEnergy.
    // Example: If you have a Blueprint event dispatcher for UI updates:
    // OnEnergyChangedDelegate.Broadcast(CurrentEnergy, MaxEnergy);
    UE_LOG(LogSolaraqMovement, VeryVerbose, TEXT("CLIENT OnRep_CurrentEnergy: %.2f"), CurrentEnergy);
}

void ASolaraqShipBase::OnRep_IsBoosting()
{
    // Called automatically on clients when bIsBoosting is updated by the server.
    // Use this to trigger cosmetic effects (e.g., thruster visuals change).
    // Example:
    // UpdateBoostEffects(bIsBoosting);
    UE_LOG(LogSolaraqMovement, VeryVerbose, TEXT("CLIENT OnRep_IsBoosting: %d"), bIsBoosting);
}

void ASolaraqShipBase::Server_RequestFire_Implementation()
{
    PerformFireWeapon(); // Player request simply calls the core logic
}

void ASolaraqShipBase::OnRep_CurrentHealth()
{
    // Called on clients when CurrentHealth changes.
    // Use this to update UI elements like a health bar.
    NET_LOG(LogSolaraqCombat, VeryVerbose, TEXT("CLIENT OnRep_CurrentHealth: %.1f/%.1f"), CurrentHealth, MaxHealth);

    // Example: Broadcast an event for Blueprint/UI to listen to
    // OnHealthChangedDelegate.Broadcast(CurrentHealth, MaxHealth); // Need to declare this delegate first
}

void ASolaraqShipBase::OnRep_IsDead()
{
    // Called on clients when bIsDead changes (primarily when it becomes true)
    NET_LOG(LogSolaraqCombat, Log, TEXT("CLIENT OnRep_IsDead: %d"), bIsDead);

    if (bIsDead)
    {
        // Ensure effects/state changes from HandleDestruction/Multicast are applied if missed
        if (ShipMeshComponent && ShipMeshComponent->IsVisible())
        {
            ShipMeshComponent->SetVisibility(false, true);
            NET_LOG(LogSolaraqCombat, Verbose, TEXT("Client %s hiding mesh in OnRep_IsDead."), *GetName());
        }
        // Ensure collision is off
        SetActorEnableCollision(ECollisionEnabled::NoCollision);
        if (CollisionAndPhysicsRoot) CollisionAndPhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (ShipMeshComponent) ShipMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        // Optionally stop ticking locally if needed
        // SetActorTickEnabled(false);
    }
    // else // Optional: Handle resurrection logic if applicable
    // {
    //     SetActorEnableCollision(ECollisionEnabled::QueryAndPhysics); // Or original profile
    //     if (ShipMeshComponent) ShipMeshComponent->SetVisibility(true, true);
    // }
}

void ASolaraqShipBase::HandleDestruction()
{
    // Should only be called on the Server
    if (!HasAuthority() || bIsDead) // Prevent multiple calls or client execution
    {
        return;
    }

    NET_LOG(LogSolaraqCombat, Log, TEXT("Ship Destroyed!"));

    // 1. Set the dead state (this will replicate via OnRep_IsDead)
    bIsDead = true;
    // Force Net Update to try and get the state change out quickly. May not be needed depending on NetUpdateFrequency.
    // ForceNetUpdate(); // Consider if needed, can increase bandwidth.

    // 2. Immediately trigger visual/audio effects on all clients via Multicast
    Multicast_PlayDestructionEffects();

    // 3. Disable ship functionality on the server
    // Stop physics simulation and movement
    if (CollisionAndPhysicsRoot)
    {
        CollisionAndPhysicsRoot->SetSimulatePhysics(false);
        CollisionAndPhysicsRoot->SetPhysicsLinearVelocity(FVector::ZeroVector); // Stop immediately
        CollisionAndPhysicsRoot->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }
    SetActorTickEnabled(false); // Stop ticking

    // Disable collision so destroyed ship doesn't block others
    SetActorEnableCollision(ECollisionEnabled::NoCollision);
    if (CollisionAndPhysicsRoot) CollisionAndPhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (ShipMeshComponent) ShipMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);


    // Detach player controller if this was possessed by a player
    AController* CurrentController = GetController();
    if (CurrentController)
    {
       CurrentController->UnPossess();
       // Note: The GameMode is typically responsible for handling respawn logic after unpossession.
    }

    // 4. Notify GameMode (optional but good practice)
    // AGamemodeBase* GM = GetWorld()->GetAuthGameMode();
    // if (GM) { GM->PawnKilled(this); } // You'd need to add PawnKilled to your GameMode

    // 5. Set the actor to be destroyed after a delay (gives time for effects/replication)
    SetLifeSpan(5.0f); // Actor will be automatically destroyed after 5 seconds
}

