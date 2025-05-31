// SolaraqShipBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"
#include "TimerManager.h"
#include "Components/DockingPadComponent.h" // Includes EDockingStatus
#include "SolaraqShipBase.generated.h" // Must be last include

// Forward declarations
class ASolaraqHomingProjectile;
class ASolaraqProjectile;
class UStaticMeshComponent;
class USpringArmComponent;
class USphereComponent;
class USceneComponent;
class UDockingPadComponent;
class UDamageType; // Included as it was in TakeDamage parameters

/**
 * @brief Abstract base class for all player-controlled and AI ships in Solaraq.
 */
UCLASS(Abstract)
class SOLARAQ_API ASolaraqShipBase : public APawn, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	// --- CONSTRUCTION & LIFECYCLE ---
	ASolaraqShipBase();
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- PAWN & ACTOR INTERFACE ---
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// --- TEAM & AFFILIATION ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Team")
	FGenericTeamId TeamId = FGenericTeamId(0); // Default Player Team ID = 0
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	// --- INTERACTION & TRANSITIONS ---
	/** Called by PlayerController when Interact is pressed (e.g., to initiate docking transition). */
	void RequestInteraction();

	/** Overrides the default character level to transition to. Set per ship instance if needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Transition")
	FName CharacterLevelOverrideName;

	/** Server RPC called by this ship to request its controlling player to transition to a character level. */
	UFUNCTION(Server, Reliable)
	void Server_RequestTransitionToCharacterLevel(FName TargetLevel, FName DockingPadID);

	// --- PUBLIC GETTERS ---
	UFUNCTION(BlueprintPure, Category = "Solaraq|Weapon")
	float GetProjectileMuzzleSpeed() const { return ProjectileMuzzleSpeed; }

	UFUNCTION(BlueprintPure, Category = "Solaraq|Docking")
	bool IsDockedToPadID(FName PadUniqueID) const;

	FORCEINLINE USphereComponent* GetCollisionAndPhysicsRoot() const { return CollisionAndPhysicsRoot; }
	FORCEINLINE UStaticMeshComponent* GetShipMeshComponent() const { return ShipMeshComponent; }
	FORCEINLINE USpringArmComponent* GetSpringArmComponent() const { return SpringArmComponent; }

	UFUNCTION(BlueprintPure, Category = "Solaraq|Boost")
	float GetCurrentEnergy() const { return CurrentEnergy; }
	UFUNCTION(BlueprintPure, Category = "Solaraq|Boost")
	float GetMaxEnergy() const { return MaxEnergy; }
	UFUNCTION(BlueprintPure, Category = "Solaraq|Boost")
	bool IsBoosting() const { return bIsBoosting; }

	UFUNCTION(BlueprintPure, Category = "Solaraq|Docking")
	bool IsShipDocked() const { return CurrentDockingStatus == EDockingStatus::Docked; }
	UFUNCTION(BlueprintPure, Category = "Solaraq|Docking")
	bool IsShipDockedOrDocking() const;
	UFUNCTION(BlueprintPure, Category = "Solaraq|Docking")
	bool IsDockedToPad(const UDockingPadComponent* Pad) const;
	UFUNCTION(BlueprintPure, Category = "Solaraq|Docking")
	UDockingPadComponent* GetActiveDockingPad() const { return ActiveDockingPad; }
	UFUNCTION(BlueprintPure, Category = "Solaraq|Docking") // BlueprintPure if you want to call it from BP easily
	FRotator GetActualDockingTargetRelativeRotation() const;

	UFUNCTION(BlueprintPure, Category = "Solaraq|Health")
	float GetHealthPercentage() const;
	UFUNCTION(BlueprintPure, Category = "Solaraq|Health")
	bool IsDead() const { return bIsDead; }
	
	UFUNCTION(BlueprintPure, Category = "Solaraq|Visuals")
	bool IsVisuallyScaledClient() const { return !FMath::IsNearlyEqual(LastAppliedScaleFactor, 1.0f); }

	UFUNCTION(BlueprintPure, Category = "Solaraq|Shield")
	float GetCurrentShieldEnergy() const { return CurrentShieldEnergy; }
	UFUNCTION(BlueprintPure, Category = "Solaraq|Shield")
	float GetMaxShieldEnergy() const { return MaxShieldEnergy; }
	UFUNCTION(BlueprintPure, Category = "Solaraq|Shield")
	bool IsShieldActive() const { return bIsShieldActive; }

	// --- SERVER RPCs FOR CLIENT INPUT ---
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Solaraq|Input")
	void Server_SendMoveForwardInput(float Value);
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Solaraq|Input")
	void Server_SendTurnInput(float Value);
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Solaraq|Input")
	void Server_SetAttemptingBoost(bool bAttempting);
 	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Solaraq|Weapon")
	void Server_RequestFire();
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Solaraq|Weapon")
    void Server_RequestFireHomingMissileAtTarget(AActor* TargetToShootAt);
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Solaraq|Shield")
	void Server_RequestToggleShield();
	
	// --- DOCKING RPCs & FUNCTIONS ---
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Solaraq|Docking")
	void Server_RequestDockWithPad(UDockingPadComponent* PadToDockWith);
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Solaraq|Docking")
	void Server_RequestUndock();
	UFUNCTION(BlueprintCallable, Category = "Solaraq|Docking") // Called by GameMode post-load
	void Server_AttemptReestablishDockingAfterLoad();

	// --- CELESTIAL BODY SCALING INTERACTION (SERVER-SIDE CONTROL) ---
	/** Called by CelestialBodyBase on the server to indicate if this ship is under its scaling effect. */
	void SetUnderScalingEffect_Server(bool bIsBeingScaled);
	/** Called by CelestialBodyBase on the server to update the ship's current effective scale factor for physics. */
	void SetEffectiveScaleFactor_Server(float NewScaleFactor);

	// --- VISUAL SCALING (CLIENT RPCs) ---
	UFUNCTION(Client, Reliable)
	void Client_SetVisualScale(float NewScaleFactor);
	UFUNCTION(Client, Reliable)
	void Client_ResetVisualScale();

	// --- INVENTORY & RESOURCES ---
	/** Public function to allow the controller (or input binding) to update the turn input state */
	void SetTurnInputForRoll(float TurnValue);


protected:
	// --- CORE COMPONENTS ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> CollisionAndPhysicsRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
	TObjectPtr<UStaticMeshComponent> ShipMeshComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
	TObjectPtr<UStaticMeshComponent> ShieldMeshComponent; 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
	TObjectPtr<USpringArmComponent> SpringArmComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> MuzzlePoint;

	// --- MOVEMENT & PHYSICS ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Movement", meta = (ForceUnits="cm/s^2 * kg?"))
	float ThrustForce = 140000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Movement")
	float TurnSpeed = 120.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Dampening = 0.05f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Movement", meta = (ClampMin = "0.0", ForceUnits="cm/s"))
	float NormalMaxSpeed = 2000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Movement", meta = (ClampMin = "0.0", ForceUnits="cm"))
	float BoostMaxSpeed = 6000.0f;

	// --- MOVEMENT SCALING FACTORS ---
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Movement|Scaling", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MinScaleSpeedReductionFactor = 0.7f;
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Movement|Scaling", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MinScaleThrustReductionFactor = 0.7f;

	/** Server-side: Current effective scale factor applied by celestial bodies. 1.0 means no scaling. */
	float CurrentEffectiveScaleFactor_Server = 1.0f;
	/** Server-side flag: True if a celestial body is currently applying a scaling effect to this ship. */
	bool bIsUnderScalingEffect_Server = false;

	// --- VISUALS & ROLL ---
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Visuals")
	float MaxTurnRollAngle = 30.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Visuals")
	float RollInterpolationSpeed = 6.0f;
	UPROPERTY(Transient, ReplicatedUsing = OnRep_TurnInputForRoll)
	float CurrentTurnInputForRoll = 0.0f;
	UPROPERTY(Transient)
	float CurrentVisualRoll = 0.0f;
	UPROPERTY(Transient)
	FVector DefaultVisualMeshScale = FVector::OneVector;
	UPROPERTY(Transient)
	float LastAppliedScaleFactor = 1.0f;

	// --- WEAPONS: STANDARD PROJECTILE ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Weapon")
	TSubclassOf<ASolaraqProjectile> ProjectileClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Weapon", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float ProjectileMuzzleSpeed = 8000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Weapon", meta = (ClampMin = "0.05"))
	float FireRate = 0.5f;
	UPROPERTY(Transient)
	float LastFireTime = -1.0f;

	// --- WEAPONS: HOMING MISSILE ---
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Weapon|HomingMissile")
	TSubclassOf<ASolaraqHomingProjectile> HomingProjectileClass;
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Weapon|HomingMissile")
	float HomingMissileFireRate = 2.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Weapon|HomingMissile")
	float HomingMissileLaunchSpeed = 4000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Weapon|HomingMissile")
	float MaxHomingTargetRange = 10000.0f;
	UPROPERTY(Transient)
	float LastHomingFireTime = -1.0f;

	// --- BOOST SYSTEM ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Solaraq|Boost", meta = (ClampMin = "0.0"))
	float MaxEnergy = 100.0f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solaraq|Boost", ReplicatedUsing = OnRep_CurrentEnergy)
	float CurrentEnergy = 100.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Boost", meta = (ClampMin = "0.0"))
	float EnergyDrainRate = 25.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Boost", meta = (ClampMin = "0.0"))
	float EnergyRegenRate = 15.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Boost", meta = (ClampMin = "0.0"))
	float EnergyRegenDelay = 1.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Boost", meta = (ClampMin = "1.0"))
	float BoostThrustMultiplier = 3.0f;
	UPROPERTY(Replicated)
	bool bIsAttemptingBoostInput = false;
	UPROPERTY(ReplicatedUsing = OnRep_IsBoosting)
	bool bIsBoosting = false;
	UPROPERTY(Transient)
	float LastBoostStopTime = -1.0f;

	
	// --- SHIELD SYSTEM ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Solaraq|Shield", meta = (ClampMin = "0.0"))
	float MaxShieldEnergy = 100.0f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solaraq|Shield", ReplicatedUsing = OnRep_CurrentShieldEnergy)
	float CurrentShieldEnergy = 100.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Solaraq|Shield", meta = (DisplayName = "Max Shield HP", ClampMin = "0.0"))
	float MaxShieldStrength; // Max HitPoints of the shield when active
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Shield", ReplicatedUsing = OnRep_CurrentShieldStrength, meta = (DisplayName = "Current Shield HP"))
	float CurrentShieldStrength; // Current HitPoints of the shield when active
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Shield", meta = (ClampMin = "0.0", ToolTip = "Energy drained per second while shield is active."))
	float ShieldEnergyDrainRate = 0.833f; // Approx 100 energy / 120 seconds
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Shield", meta = (ClampMin = "0.0", ToolTip = "Energy regenerated per second when shield is down and regen delay has passed."))
	float ShieldEnergyRegenRate = 1.666f; // Approx 2 * DrainRate
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Shield", meta = (ClampMin = "0.0", ToolTip = "Delay in seconds after shield deactivation before regeneration starts."))
	float ShieldRegenDelay = 3.0f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solaraq|Shield", ReplicatedUsing = OnRep_IsShieldActive)
	bool bIsShieldActive = false;
	UPROPERTY(Transient)
	float LastShieldDeactivationTime = -1.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Shield", meta = (ClampMin = "0.0", ToolTip = "Minimum energy required to activate the shield. 0 means can activate anytime if not on cooldown."))
	float MinEnergyToActivateShield = 1.0f; // Example: require at least 1 energy to turn on
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Shield", meta = (ClampMin = "0.0", ToolTip = "Cooldown in seconds after shield is manually deactivated or broken before it can be reactivated."))
	float ShieldActivationCooldown = 1.0f;
	float ShieldTimerUpdateInterval = 0.1f;
	
	FTimerHandle TimerHandle_ShieldDrain;
	FTimerHandle TimerHandle_ShieldRegen;
	FTimerHandle TimerHandle_ShieldRegenDelayCheck;
	

	// --- HEALTH & DESTRUCTION ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Health", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float MaxHealth = 100.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Health", ReplicatedUsing = OnRep_CurrentHealth, meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Health", ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	// --- DOCKING SYSTEM ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solaraq|Docking", ReplicatedUsing = OnRep_DockingStateChanged)
	EDockingStatus CurrentDockingStatus = EDockingStatus::None;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solaraq|Docking", ReplicatedUsing = OnRep_DockingStateChanged)
	TObjectPtr<UDockingPadComponent> ActiveDockingPad;
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Docking")
	FVector DockingTargetRelativeLocation = FVector::ZeroVector;
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Docking")
	FRotator DockingTargetRelativeRotation = FRotator::ZeroRotator; // This is the desired *final* relative rotation.
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Docking", meta = (ClampMin = "0.1"))
	float DockingLerpSpeed = 5.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Docking")
	float DockingCooldownDuration = 2.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Docking")
	float UndockFromThrustGracePeriod = 0.5f;

	// --- DOCKING INTERNAL STATE (Server-Side, Transient) ---
	UPROPERTY(Transient)
	bool bIsLerpingToDockPosition = false;
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> LerpAttachTargetComponent;
	UPROPERTY(Transient)
	float LastUndockTime = -1.0f;
	UPROPERTY(Transient)
	float CurrentDockingStartTime = -1.0f;
	/** Stores the actual target relative rotation calculated at the start of docking, used for lerping. */
	UPROPERTY(Transient)
	FRotator ActualDockingTargetRelativeRotation;

	// --- INVENTORY & RESOURCES (REPLICATED) ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_IronCount, Category = "Solaraq|Inventory")
	int32 CurrentIronCount = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_CrystalCount, Category = "Solaraq|Inventory")
	int32 CurrentCrystalCount = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_StandardAmmo, Category = "Solaraq|Inventory")
	int32 CurrentStandardAmmo = 100;

	// --- PROTECTED CORE LOGIC ---
	virtual void PerformFireWeapon();
	virtual void PerformFireHomingMissile(AActor* HomingTarget);
	void ProcessMoveForwardInput(float Value);
	void ProcessTurnInput(float Value);
	void ClampVelocity();
	virtual void HandleDestruction();
	void ApplyVisualScale(float ScaleFactor);

	// --- PROTECTED DOCKING LOGIC ---
	void PerformDockingAttachmentToPad(UDockingPadComponent* Pad);
	void PerformUndockingDetachmentFromPad();
	virtual void Internal_DisableSystemsForDocking();
	virtual void Internal_EnableSystemsAfterUndocking();

	void Server_ActivateShield();
	void Server_DeactivateShield(bool bForcedByEmptyOrBreak, bool bSkipCooldown = false);
	void UpdateShieldVisuals();
	void Server_ProcessShieldDrain();
	void Server_TryStartShieldRegenTimer(); // Renamed for clarity
	void Server_ProcessShieldRegen();
	void ClearAllShieldTimers();
	
	// --- REPLICATION NOTIFIERS (OnRep_ functions) ---
	UFUNCTION()
	virtual void OnRep_CurrentHealth();
	UFUNCTION()
	virtual void OnRep_IsDead();
	UFUNCTION()
	virtual void OnRep_CurrentEnergy();
	UFUNCTION()
	virtual void OnRep_IsBoosting();
	UFUNCTION()
	virtual void OnRep_DockingStateChanged();
	UFUNCTION()
	virtual void OnRep_TurnInputForRoll();
	UFUNCTION()
	void OnRep_IronCount();
	UFUNCTION()
	void OnRep_CrystalCount();
	UFUNCTION()
	void OnRep_StandardAmmo();
	UFUNCTION()
	void OnRep_CurrentShieldEnergy();
	UFUNCTION()
	void OnRep_IsShieldActive();
	UFUNCTION()
	virtual void OnRep_CurrentShieldStrength();

	// --- BLUEPRINT EVENTS & MULTICASTS---
	UFUNCTION(BlueprintImplementableEvent, Category = "Solaraq|Inventory")
	void OnInventoryUpdated();
	UFUNCTION(NetMulticast, Unreliable) // Changed to NetMulticast, Unreliable is fine for effects
	void Multicast_PlayDestructionEffects();
	UFUNCTION(NetMulticast, Unreliable, Category = "Solaraq|Shield")
	void Multicast_PlayShieldActivationEffects();
	UFUNCTION(NetMulticast, Unreliable, Category = "Solaraq|Shield")
	void Multicast_PlayShieldDeactivationEffects(bool bWasBrokenOrEmptied);
	UFUNCTION(NetMulticast, Unreliable, Category = "Solaraq|Shield")
	void Multicast_PlayShieldImpactEffects(FVector ImpactLocation, float DamageAbsorbed);
};