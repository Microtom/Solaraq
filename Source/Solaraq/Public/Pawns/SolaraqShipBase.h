// SolaraqShipBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"
#include "Components/DockingPadComponent.h" // Includes EDockingStatus
#include "SolaraqShipBase.generated.h" // Must be last include

class ASolaraqHomingProjectile;
class ASolaraqProjectile;
// Forward declarations (best practice)
class UStaticMeshComponent;
class USpringArmComponent;
class USphereComponent;
class UDamageType;
class USceneComponent;
class UProjectileMovementComponent;
class AActor;
class UDockingPadComponent; // Forward declare

// class UCameraComponent; // If camera is added later

/**
 * @brief Abstract base class for all player-controlled and AI ships in Solaraq.
 *
 * Handles core functionalities common to all ships:
 * - Physics-based movement: Thrust, Turning, Velocity Clamping, Dampening.
 * - Boost System: Energy management (drain/regen), speed increase.
 * - Health & Damage: Taking damage, destruction state, replication.
 * - Celestial Body Interaction: Receiving scale commands from CelestialBodyBase.
 * - Docking: Interaction with DockingPadComponent, state management, attachment.
 * - Basic Replication: Uses standard pawn replication for movement, replicates vital stats (Health, Energy, Boost State, Docked State).
 *
 * Designed to be inherited by Blueprint classes (e.g., BP_PlayerShip, BP_EnemyShip_TypeA)
 * which will define specific meshes, stats, weapon systems, and input handling.
 * Physics simulation is handled by the CollisionAndPhysicsRoot (SphereComponent).
 * The ShipMeshComponent is purely visual and attached to the physics root.
 */
UCLASS(Abstract) // Mark as Abstract so you can't place this base class directly in the world
class SOLARAQ_API ASolaraqShipBase : public APawn, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	/** Constructor */
	ASolaraqShipBase();

	//~ Begin AActor Interface
	/** Called every frame. Handles server-side boost logic, energy management, and velocity clamping. */
	virtual void Tick(float DeltaTime) override;
	/** Called when the game starts or when spawned. Initializes health, energy, default scale. */
	virtual void BeginPlay() override;
	/** Returns properties that are replicated for the lifetime of the actor channel */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Handles receiving damage, updating health (Server authoritative), and triggering destruction. */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	//~ End AActor Interface

	//~ Begin APawn Interface
	/** Called to bind functionality to input. Base implementation does nothing; override in derived classes. */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	//~ End APawn Interface

	// --- Generic Team Interface ---
	/** Assigns a team ID for affiliation checks */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Team") // Make it configurable per ship type if needed
	FGenericTeamId TeamId = FGenericTeamId(0); // Default Player Team ID = 0

	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	// --- End Generic Team Interface ---

	// Getter for projectile speed used by AI prediction
	UFUNCTION(BlueprintPure, Category="Solaraq|Weapon") // BlueprintPure means it doesn't change state
	float GetProjectileMuzzleSpeed() const { return ProjectileMuzzleSpeed; }

	// Called by PlayerController when Interact is pressed
	void RequestInteraction();

	// UPROPERTY specific to storing which pad requested the level change.
	UPROPERTY(BlueprintReadWrite, Category = "Solaraq|Docking")
	FName CharacterLevelOverrideName;
	
protected:
	// --- Components ---

	/** The Sphere Component that handles collision and physics simulation, acting as the root */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> CollisionAndPhysicsRoot;

	/** The main visual representation of the ship. Attached to CollisionAndPhysicsRoot, no physics/collision itself. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
	TObjectPtr<UStaticMeshComponent> ShipMeshComponent;

	/** Spring arm for attaching cameras or other components with smooth interpolation. Typically points down for top-down view. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
	TObjectPtr<USpringArmComponent> SpringArmComponent;

	/** Scene component representing the muzzle point for firing projectiles. Attach in constructor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> MuzzlePoint;

	// --- Movement Properties ---

	/** Base force applied for forward/backward thrust (scaled by input). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Movement", meta = (ForceUnits="cm/s^2 * kg?"))
	float ThrustForce = 140000.0f;

	/** Turning speed in degrees per second (scaled by input). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Movement")
	float TurnSpeed = 120.0f;

	/** Linear damping applied to the physics body to simulate friction/drag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Dampening = 0.05f;

	// --- Visual Roll ---
	/** Maximum roll angle (degrees) when turning at full speed. */
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Visuals")
	float MaxTurnRollAngle = 30.0f;

	/** How quickly the ship interpolates to the target roll angle. Higher = faster. */
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Visuals")
	float RollInterpolationSpeed = 6.0f;

	/** Current target roll angle based on input. Updated by input processing. */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_TurnInputForRoll)
	float CurrentTurnInputForRoll = 0.0f;

	/** The current visually applied roll angle */
	UPROPERTY(Transient)
	float CurrentVisualRoll = 0.0f;

	// Replication Notifier function declaration
	UFUNCTION()
	virtual void OnRep_TurnInputForRoll();

	// --- Inventory / Resources ---

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_IronCount, Category = "Inventory")
	int32 CurrentIronCount = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_CrystalCount, Category = "Inventory")
	int32 CurrentCrystalCount = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_StandardAmmo, Category = "Inventory")
	int32 CurrentStandardAmmo = 100; // Example starting ammo

	UFUNCTION() // Needed for ReplicatedUsing
	void OnRep_IronCount();

	UFUNCTION() // Needed for ReplicatedUsing
	void OnRep_CrystalCount();

	UFUNCTION() // Needed for ReplicatedUsing
	void OnRep_StandardAmmo();

	// Blueprint event for UI updates (Optional but recommended)
	UFUNCTION(BlueprintImplementableEvent, Category = "Solaraq|Inventory")
	void OnInventoryUpdated(); // Call this inside OnRep functions
	
public:
	/**
	 * Called by a pickup actor on the server when this ship collects it.
	 * @param PickupType The type of item being collected.
	 * @param Quantity The amount of the item.
	 * @return True if the pickup was successfully processed, false otherwise (e.g., inventory full).
	 */
//	UFUNCTION(BlueprintCallable, Category = "Pickup") // Can be called from BP if needed
//	virtual bool CollectPickup(EPickupType PickupType, int32 Quantity);
	
	/** Public function to allow the controller (or input binding) to update the turn input state */
	void SetTurnInputForRoll(float TurnValue);
	
protected: // Keep protected or make EditDefaultsOnly
	// --- Weapon Properties ---
	/** The Blueprint class of the projectile this ship fires. Assign BP_Bullet (or similar) in derived Blueprint Defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Weapon")
	TSubclassOf<ASolaraqProjectile> ProjectileClass;

	/** The base speed imparted to the projectile relative to the muzzle direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Weapon", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float ProjectileMuzzleSpeed = 8000.0f;

	/** How often the ship can fire (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Weapon", meta = (ClampMin = "0.05"))
	float FireRate = 0.5f;

	/** Internal state: Timestamp of the last successful weapon fire. */
	UPROPERTY(Transient)
	float LastFireTime = -1.0f;

	// --- Core Firing Logic ---
	/**
	 * Performs the actual weapon firing logic (Server-side).
	 * Checks cooldown, spawns projectile, calculates velocity including ship's velocity.
	 * Designed to be called by AI or Player request functions.
	 */
	virtual void PerformFireWeapon();
	
	// --- Speed Clamping ---

	/** Maximum speed the ship can reach without boosting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Movement", meta = (ClampMin = "0.0", ForceUnits="cm/s"))
	float NormalMaxSpeed = 2000.0f;

	/** Maximum speed the ship can reach while boosting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Movement", meta = (ClampMin = "0.0", ForceUnits="cm"))
	float BoostMaxSpeed = 6000.0f;

	// --- Boost System ---

	/** Maximum amount of boost energy the ship can store. Replicated initial-only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Solaraq|Boost", meta = (ClampMin = "0.0"))
	float MaxEnergy = 100.0f;

	/** Current boost energy level. Replicated with notification. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solaraq|Boost", ReplicatedUsing = OnRep_CurrentEnergy)
	float CurrentEnergy = 100.0f;

	/** Energy consumed per second while boosting is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Boost", meta = (ClampMin = "0.0"))
	float EnergyDrainRate = 25.0f;

	/** Energy regenerated per second when not boosting (after delay). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Boost", meta = (ClampMin = "0.0"))
	float EnergyRegenRate = 15.0f;

	/** Delay in seconds after stopping boost before energy regeneration begins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Boost", meta = (ClampMin = "0.0"))
	float EnergyRegenDelay = 1.5f;

	/** Multiplier applied to ThrustForce when boost is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Boost", meta = (ClampMin = "1.0"))
	float BoostThrustMultiplier = 3.0f; // Adjusted default for clarity

	/** Internal state: True if the player controller is currently sending a 'boost active' input signal. Replicated. */
	UPROPERTY(Replicated)
	bool bIsAttemptingBoostInput = false;

	/** Internal state: True if the ship is actually boosting (has energy and input is active). Replicated with notification. */
	UPROPERTY(ReplicatedUsing = OnRep_IsBoosting)
	bool bIsBoosting = false;

	/** Server-side timer tracking when boost was last stopped, used for regen delay. */
	UPROPERTY(Transient)
	float LastBoostStopTime = -1.0f;

	// --- Health Properties ---

	/** Maximum health points of the ship. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Health", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float MaxHealth = 100.0f;

	/** Current health points. Replicated with notification. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Health", ReplicatedUsing = OnRep_CurrentHealth, meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.f;

	/** Flag indicating if the ship has been destroyed. Replicated with notification. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Health", ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	// --- Missile Properties ---
	
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Weapon|HomingMissile")
	TSubclassOf<ASolaraqHomingProjectile> HomingProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Weapon|HomingMissile")
	float HomingMissileFireRate = 2.0f; // Seconds between shots

	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Weapon|HomingMissile")
	float HomingMissileLaunchSpeed = 4000.0f; // Speed missile is launched at

	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Weapon|HomingMissile")
	float MaxHomingTargetRange = 10000.0f; // Max range to acquire a homing target

	UPROPERTY(Transient)
	float LastHomingFireTime = -1.0f;

	void PerformFireHomingMissile(AActor* HomingTarget);
	
	// --- Docking State ---

	/** Current status of docking. Replicated with notification. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solaraq|Docking", ReplicatedUsing = OnRep_DockingStateChanged)
	EDockingStatus CurrentDockingStatus = EDockingStatus::None;

	/** Reference to the specific docking pad component we are docked/docking to. Replicated with notification. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solaraq|Docking", ReplicatedUsing = OnRep_DockingStateChanged)
	TObjectPtr<UDockingPadComponent> ActiveDockingPad;

	// --- Server-Side Movement Logic ---
protected:
	/** Applies forward/backward force based on input axis value (Server-side). */
	void ProcessMoveForwardInput(float Value);

	/** Applies turning rotation based on input axis value (Server-side). */
	void ProcessTurnInput(float Value);

	/** Helper function to clamp the ship's physics velocity to the current maximum speed (normal or boost). Called on Server. */
	void ClampVelocity();

	// --- Server RPCs from Client Input ---
public:
	/** Server RPC called by the client to forward movement input for server processing. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Solaraq|Server")
	void Server_SendMoveForwardInput(float Value);

	/** Server RPC called by the client to forward turning input for server processing. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Solaraq|Server")
	void Server_SendTurnInput(float Value);

	/** Server RPC called by the client to signal the player is holding/releasing the boost input. */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Solaraq|Server")
	void Server_SetAttemptingBoost(bool bAttempting);

 	/** Server RPC called by the client player to request firing the weapon. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Solaraq|Weapon")
	void Server_RequestFire();

	/** Server RPC called by the client player to request firing a homing missile at a target. */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Solaraq|Weapon") // Make callable for player input binding
    void Server_RequestFireHomingMissileAtTarget(AActor* TargetToShootAt);
	
protected:
	// --- Replication Notifiers (Called on Clients) ---
	/** Replication notification function for CurrentHealth. Called on Clients. */
	UFUNCTION()
	void OnRep_CurrentHealth();

	/** Replication notification function for bIsDead. Called on Clients. */
	UFUNCTION()
	void OnRep_IsDead();

	/** Replication notification function for CurrentEnergy. Called on Clients. */
	UFUNCTION()
	virtual void OnRep_CurrentEnergy();

	/** Replication notification function for bIsBoosting. Called on Clients. */
	UFUNCTION()
	virtual void OnRep_IsBoosting();

	/** Replication notification function for docking state changes. Called on Clients. */
	UFUNCTION()
	virtual void OnRep_DockingStateChanged();

	// --- Health & Destruction ---

	/** Server-side function to handle the actual destruction logic (disabling components, unpossessing, setting lifespan). */
	virtual void HandleDestruction();

	/** Multicast RPC to play cosmetic destruction effects (visuals, sound) on Server and all Clients. */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDestructionEffects();
	virtual void Multicast_PlayDestructionEffects_Implementation(); // Provide base implementation

	// --- Celestial Body Interaction ---

	/** Store the default scale of the visual mesh set in the editor/Blueprint. */
	UPROPERTY(Transient)
	FVector DefaultVisualMeshScale = FVector::OneVector;

	/** Optional: Store the last scale factor applied to avoid redundant RPC calls. */
	UPROPERTY(Transient)
	float LastAppliedScaleFactor = 1.0f;

	/** Internal helper function to apply the visual scale factor to the mesh component. */
	void ApplyVisualScale(float ScaleFactor);
public:
	/** Reliable Client RPC called by the server (from CelestialBodyBase) to apply visual scaling to the ship mesh. */
	UFUNCTION(Client, Reliable)
	void Client_SetVisualScale(float NewScaleFactor);
	
	/** Reliable Client RPC called by the server (from CelestialBodyBase) to reset visual scaling to default. */
	UFUNCTION(Client, Reliable)
	void Client_ResetVisualScale();
	
	// --- Docking Logic ---

public:
	/** Server RPC called by client overlap or AI to request docking with a specific pad. */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Solaraq|Docking")
	void Server_RequestDockWithPad(UDockingPadComponent* PadToDockWith);

	/** Server RPC called by client input or logic to request undocking. Handles state changes and detachment. */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Solaraq|Docking")
	void Server_RequestUndock();
protected:
	/** Server-side helper function to handle the actual physics disabling and component attachment during docking. */
	void PerformDockingAttachmentToPad(UDockingPadComponent* Pad);

	/** Server-side helper function to handle the actual component detachment and physics re-enabling during undocking. */
	void PerformUndockingDetachmentFromPad();

	/** Server-side helper function to disable relevant ship systems (input, boost, etc.) when docked. */
	virtual void Internal_DisableSystemsForDocking();

	/** Server-side helper function to re-enable relevant ship systems after undocking. */
	virtual void Internal_EnableSystemsAfterUndocking();

	/** Server-side flag: True if a celestial body is currently applying a scaling effect to this ship. */
	bool bIsUnderScalingEffect_Server = false;

	/** Server-side: Current effective scale factor applied by celestial bodies. 1.0 means no scaling. */
	float CurrentEffectiveScaleFactor_Server = 1.0f;

	/**
	 * Defines how much the ship's max speed is reduced at its smallest scale (MinShipScaleFactor).
	 * 0.0 = Max speed becomes 0 at MinShipScaleFactor.
	 * 0.5 = Max speed is halved at MinShipScaleFactor.
	 * 1.0 = No speed reduction due to scaling (default behavior).
	 * Values above 1.0 would mean speed increases (unlikely desired).
	 * This factor is used to interpolate the speed reduction.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Movement|Scaling", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MinScaleSpeedReductionFactor = 0.7f; // e.g., at smallest scale, speed is 70% of normal. (So 30% reduction)

	/**
	 * Defines how much the ship's thrust is reduced at its smallest scale (MinShipScaleFactor).
	 * Similar to MinScaleSpeedReductionFactor.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Movement|Scaling", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MinScaleThrustReductionFactor = 0.7f; // e.g., at smallest scale, thrust is 70% of normal.

public:
	// --- Public Getters ---

	/** Gets the physics root component (SphereComponent). */
	FORCEINLINE USphereComponent* GetCollisionAndPhysicsRoot() const { return CollisionAndPhysicsRoot; }

	/** Gets the visual static mesh component. */
	FORCEINLINE UStaticMeshComponent* GetShipMeshComponent() const { return ShipMeshComponent; }

	/** Gets the spring arm component. */
	FORCEINLINE USpringArmComponent* GetSpringArmComponent() const { return SpringArmComponent; }

	/** Gets the current boost energy level. */
	UFUNCTION(BlueprintPure, Category = "Solaraq|Boost")
	float GetCurrentEnergy() const { return CurrentEnergy; }

	/** Gets the maximum boost energy capacity. */
	UFUNCTION(BlueprintPure, Category = "Solaraq|Boost")
	float GetMaxEnergy() const { return MaxEnergy; }

	/** Returns true if the ship is currently actively boosting (energy > 0 and input active). */
	UFUNCTION(BlueprintPure, Category = "Solaraq|Boost")
	bool IsBoosting() const { return bIsBoosting; }

	/** Returns true if the ship is currently docked. */
	UFUNCTION(BlueprintPure, Category = "Solaraq|Docking")
	bool IsShipDocked() const { return CurrentDockingStatus == EDockingStatus::Docked; }

	/** Returns true if the ship is currently docked or in the process of docking/undocking. */
	UFUNCTION(BlueprintPure, Category = "Solaraq|Docking")
	bool IsShipDockedOrDocking() const;

	/** Returns true if the ship is docked specifically to the given pad component. */
	UFUNCTION(BlueprintPure, Category = "Solaraq|Docking")
	bool IsDockedToPad(const UDockingPadComponent* Pad) const;

	/** Called by CelestialBodyBase on the server to indicate if this ship is under its scaling effect. */
	void SetUnderScalingEffect_Server(bool bIsBeingScaled);

	/** Called by CelestialBodyBase on the server to update the ship's current effective scale factor for physics. */
	void SetEffectiveScaleFactor_Server(float NewScaleFactor);
	
	/** Client-side check: Returns true if the ship's visual mesh is currently scaled (not 1.0f). */
	UFUNCTION(BlueprintPure, Category = "Solaraq|Visuals")
	bool IsVisuallyScaledClient() const { return !FMath::IsNearlyEqual(LastAppliedScaleFactor, 1.0f); }
	
	/** Gets the docking pad component the ship is currently associated with (docked or attempting), if any. */
	UFUNCTION(BlueprintPure, Category = "Solaraq|Docking")
	UDockingPadComponent* GetActiveDockingPad() const { return ActiveDockingPad; }

	/** Gets the current health as a fraction of maximum health (0.0 to 1.0). */
	UFUNCTION(BlueprintPure, Category = "Solaraq|Health")
	float GetHealthPercentage() const;

	/** Returns true if the ship's health is at or below zero and the destruction process has started. */
	UFUNCTION(BlueprintPure, Category = "Solaraq|Health")
	bool IsDead() const { return bIsDead; }

	/** Target relative location for the ship when docking (usually FVector::ZeroVector). */
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Docking")
	FVector DockingTargetRelativeLocation = FVector::ZeroVector;

	/** Target relative rotation for the ship when docking (usually FRotator::ZeroRotator). */
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Docking")
	FRotator DockingTargetRelativeRotation = FRotator::ZeroRotator;

	/** Speed of the lerp towards the docking target transform. Higher is faster. */
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Docking", meta = (ClampMin = "0.1"))
	float DockingLerpSpeed = 5.0f;

	/** Server-side: Flag to indicate if the docking lerp is currently active. */
	UPROPERTY(Transient) // No need to replicate, server controls position
	bool bIsLerpingToDockPosition = false;

	/** Server-side: Store the component to attach to during lerp. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> LerpAttachTargetComponent;

	/** Minimum time (seconds) after undocking before the ship can attempt to dock again. */
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Docking")
	float DockingCooldownDuration = 2.0f;

	/** Server-side: Timestamp when the last undock completed. Used for docking cooldown. */
	UPROPERTY(Transient)
	float LastUndockTime = -1.0f;

	/** Minimum time (seconds) after initiating a dock before forward thrust will trigger an undock.
	 *  This prevents immediate undock if thrusting into the pad. */
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Docking")
	float UndockFromThrustGracePeriod = 0.5f; // Small grace period

	/** Server-side: Timestamp when the current docking process (lerp) started. */
	UPROPERTY(Transient)
	float CurrentDockingStartTime = -1.0f;

	UPROPERTY(Transient)
	FRotator ActualDockingTargetRelativeRotation;
};