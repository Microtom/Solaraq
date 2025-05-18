#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.h" // For team-based targeting
#include "SolaraqTurretBase.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class ASolaraqProjectile; // Forward declaration from your project
class USoundCue;
class UParticleSystem;
class ASolaraqShipBase; // For getting owner velocity

UCLASS(Blueprintable, BlueprintType)
class SOLARAQ_API ATurretBase : public AActor, public IGenericTeamAgentInterface
{
    GENERATED_BODY()

public:
    ATurretBase();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override; // Added for replication

public:
    virtual void Tick(float DeltaTime) override;

    // --- Components ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    USceneComponent* RootSceneComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    USceneComponent* TurretYawPivot; 

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* TurretGunMesh;  

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components", meta = (AllowPrivateAccess = "true")) 
    USceneComponent* MuzzleLocationComponent; 

    // --- Configuration: Projectile & Firing ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Configuration", meta = (DisplayName = "Projectile Blueprint"))
    TSubclassOf<ASolaraqProjectile> ProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Configuration", meta = (ClampMin = "0.1", UIMin = "0.1"))
    float FireRate = 1.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Configuration", meta = (ClampMin = "100.0", UIMin = "100.0"))
    float ProjectileSpeedOverride = 0.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Configuration", meta = (ClampMin = "0.01", UIMin = "0.01"))
    float InitialFireDelay = 0.0f; 

    // --- Configuration: Targeting & Rotation ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Configuration", meta = (ClampMin = "10.0", UIMin = "10.0"))
    float TurnRateDegreesPerSecond = 90.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Configuration", meta = (ClampMin = "100.0", UIMin = "100.0"))
    float TargetingRange = 3000.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Configuration", meta = (ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "180.0", ToolTip = "0 for 360 degree freedom, >0 for limited arc (e.g. 90 for +/-90 deg)."))
    float MaxYawRotationAngleDegrees = 0.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Configuration", meta = (ClampMin = "0.0", ClampMax = "45.0"))
    float FiringToleranceAngleDegrees = 5.0f;

    // --- Configuration: Team ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Team")
    FGenericTeamId TeamId = FGenericTeamId(10); 

    // --- Optional Effects ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Effects")
    USoundCue* FireSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Effects")
    UParticleSystem* MuzzleFlashEffect;

    // --- Targeting State (Read-Only for Blueprints) ---
    UPROPERTY(BlueprintReadOnly, Category = "Turret|State")
    TWeakObjectPtr<AActor> CurrentTarget;

    UFUNCTION(BlueprintCallable, Category = "Turret")
    void SetTargetManually(AActor* NewTarget);

    UFUNCTION(BlueprintPure, Category = "Turret")
    bool HasTarget() const { return CurrentTarget.IsValid(); }

protected:
    // --- Internal State ---
    float ActualProjectileSpeed = 5000.0f; 
    float FireCooldownRemaining = 0.0f;
    bool bCanFire = true; 
    
    // Stores the world location the turret is currently trying to smoothly aim towards.
    FVector SmoothedAimWorldLocation;

    // Speed at which SmoothedAimWorldLocation updates towards the calculated InterceptPoint.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Configuration", meta = (ClampMin = "0.1"))
    float AimSmoothingSpeed = 5.0f; // Adjust this value to control how quickly the aim adapts
    
    // --- Replication for Smooth Rotation ---
    UPROPERTY(ReplicatedUsing = OnRep_ReplicatedYawPivotRelativeRotation)
    FRotator ReplicatedYawPivotRelativeRotation;

    UFUNCTION()
    void OnRep_ReplicatedYawPivotRelativeRotation();

    // Stores the target rotation for client-side interpolation
    FRotator ClientTargetYawPivotRelativeRotation;


    // --- Core Logic ---
    virtual void FindNewTarget();
    virtual bool IsValidTarget(AActor* TargetActor) const;
    virtual void RotateTurretTowards(const FVector& TargetWorldLocation, float DeltaTime);
    virtual void AttemptFire(const FVector& AimLocation);
    
    FVector GetShooterVelocity() const;
    FVector GetMuzzleLocation() const;
    FRotator GetMuzzleRotation() const;

    float GetDesiredYawRelativeToBase(const FVector& TargetWorldLocation) const;

public:
    // IGenericTeamAgentInterface
    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
};