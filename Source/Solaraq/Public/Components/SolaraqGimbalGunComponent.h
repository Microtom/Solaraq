// SolaraqGimbalGunComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GenericTeamAgentInterface.h" // For projectile owner
#include "SolaraqGimbalGunComponent.generated.h"

class ASolaraqProjectile;
class UStaticMeshComponent;
class APawn;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SOLARAQ_API USolaraqGimbalGunComponent : public USceneComponent, public IGenericTeamAgentInterface
{
    GENERATED_BODY()

public:
    USolaraqGimbalGunComponent();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    //~ Begin IGenericTeamAgentInterface
    virtual FGenericTeamId GetGenericTeamId() const override;
    //~ End IGenericTeamAgentInterface

    /** Sets the pawn that owns/instigates actions from this gun */
    void SetOwningPawn(APawn* NewOwningPawn);

    // --- FIRING ---
public:
    /** Client requests to fire the gun */
    UFUNCTION(BlueprintCallable, Category = "Solaraq|GimbalGun|Firing")
    void RequestFire();

protected:
    /** Server performs the actual fire logic */
    UFUNCTION(Server, Reliable)
    void Server_PerformFire();

    /** Actually spawns the projectile. Called on server. */
    void FireShot();

    /** Can the gun fire right now? (Cooldown, etc.) */
    bool CanFire() const;

    /** Gets the world transform of the muzzle point */
    FTransform GetMuzzleWorldTransform() const;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|GimbalGun|Firing")
    TSubclassOf<ASolaraqProjectile> ProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|GimbalGun|Firing", meta = (ToolTip = "Name of the socket on GunMeshComponent to fire from. If NAME_None, MuzzleOffset is used from component origin."))
    FName MuzzleSocketName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|GimbalGun|Firing", meta = (EditCondition = "MuzzleSocketName == NAME_None", EditConditionHides))
    FVector MuzzleOffset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|GimbalGun|Firing", meta = (ClampMin = "0.01", UIMin = "0.01"))
    float FireRate; // Shots per second

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|GimbalGun|Firing", meta = (ClampMin = "0.0"))
    float ProjectileMuzzleSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|GimbalGun|Firing", meta = (ClampMin = "0.0"))
    float BaseDamage; // Can be passed to projectile

protected:
    float LastFireTime;

    // --- AIMING & ROTATION ---
public:
    /** Called by owning actor (usually ship) to tell the gun where to aim. Client or Server. */
    void AimAtWorldLocation(const FVector& WorldTargetLocation);

protected:
    UFUNCTION(Server, Unreliable)
    void Server_SetDesiredYaw(float NewDesiredYaw);

    /** Applies rotation constraints to a given yaw value relative to the component's parent. */
    float GetClampedRelativeYaw(float InYaw) const;

    /** Interpolates the GunMeshComponent's yaw towards a target. Should only affect GunMeshComponent's relative rotation. */
    void UpdateGunMeshRotation(float DeltaTime);

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|GimbalGun|Visuals")
    UStaticMeshComponent* GunMeshComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|GimbalGun|Aiming", meta = (ClampMin = "1.0", UIMin = "1.0", ToolTip = "How fast the gimbal can rotate in degrees per second."))
    float MaxYawRotationSpeed; // Degrees per second

    UPROPERTY(ReplicatedUsing = OnRep_CurrentActualGimbalRelativeYaw)
    float CurrentActualGimbalRelativeYaw; // Yaw of the gimbal relative to its attachment parent's forward

    UFUNCTION()
    void OnRep_CurrentActualGimbalRelativeYaw();

protected:
    /** The yaw the client/server wants the gun to point to, relative to parent. Constraints will be applied to this by server. */
    float DesiredGimbalRelativeYaw;

    /** Smoothed target for client-side visuals, based on server updates or local input */
    float ClientVisualGimbalRelativeYaw;


    // --- CONSTRAINTS (Yaw for 2D plane) ---
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|GimbalGun|Constraints", meta = (DisplayName = "Enable Yaw Constraints"))
    bool bEnableYawConstraints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|GimbalGun|Constraints", meta = (EditCondition = "bEnableYawConstraints", UIMin = "-180.0", UIMax = "180.0", DisplayName = "Constraint Center Yaw (Relative to Parent)"))
    float ConstraintCenterRelativeYaw; // The "neutral" or "center" yaw of the constraint arc, relative to the parent component's forward.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|GimbalGun|Constraints", meta = (EditCondition = "bEnableYawConstraints", ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "180.0", DisplayName = "Max Yaw Angle From Center"))
    float MaxYawAngleFromCenter; // Max deviation from ConstraintCenterRelativeYaw. E.g., 45 means a 90-degree total arc.

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    void DrawConstraintArc() const;
#endif

private:
    UPROPERTY()
    TWeakObjectPtr<APawn> OwningPawn; // The pawn that owns this component, for instigator and team ID

    FGenericTeamId TeamId;
};