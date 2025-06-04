#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MiningLaserComponent.generated.h"

class UNiagaraComponent;
class UStaticMeshComponent; // For the laser emitter mesh (optional, could be just a scene component)
class USceneComponent;      // For defining the muzzle point
class UParticleSystem;      // For the laser beam visual (e.g., Niagara or Cascade)
class UParticleSystemComponent; // To manage the active beam particle system
class USoundBase;           // Sound for when the laser is active
class UAudioComponent;      // To manage the active laser sound
class UMiningDamageType;    // Forward declare our custom damage type

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SOLARAQ_API UMiningLaserComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMiningLaserComponent();

protected:
    //~ Begin UActorComponent Interface
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    //~ End UActorComponent Interface

public:
    // INPUT & CONTROL
    UFUNCTION(BlueprintCallable, Category = "Solaraq|MiningLaser|Control")
    void ActivateLaser(bool bNewActiveState);

    UFUNCTION(BlueprintCallable, Category = "Solaraq|MiningLaser|Control")
    void SetTargetWorldLocation(const FVector& NewTargetLocation);

    UFUNCTION(BlueprintPure, Category = "Solaraq|MiningLaser|State")
    bool IsLaserActive() const { return bLaserIsActive; }

    UFUNCTION(BlueprintPure, Category = "Solaraq|MiningLaser|State")
    FVector GetCurrentTargetWorldLocation() const { return CurrentTargetWorldLocation; }

    // CONFIGURATION
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|MiningLaser|Configuration")
    float MaxRange;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|MiningLaser|Configuration")
    float DamagePerSecond;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|MiningLaser|Configuration")
    float MaxTurnRateDegreesPerSecond; // How fast the laser emitter can rotate

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|MiningLaser|Configuration")
    TSubclassOf<UMiningDamageType> MiningDamageTypeClass; // Assign our UMiningDamageType BP here

    // VISUALS & AUDIO
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|MiningLaser|Effects")
    TObjectPtr<UParticleSystem> BeamParticleSystem; // Niagara or Cascade for the beam

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|MiningLaser|Effects")
    TObjectPtr<UParticleSystem> ImpactParticleSystem; // Effect at the point of impact

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|MiningLaser|Effects")
    TObjectPtr<USoundBase> ActiveLaserSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|MiningLaser|Effects")
    FName BeamSourceSocketName; // Optional socket on the owner to attach the beam source

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|MiningLaser|Effects")
    FName BeamTargetParameterName; // For Niagara: name of the User.BeamTarget (Vector) parameter

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|MiningLaser|Attachment", meta = (ToolTip = "The NAME of the SceneComponent on the owning actor that defines the laser's muzzle position and orientation. If set, overrides LaserMuzzleComponent direct assignment."))
    FName LaserMuzzleComponentName;
    
    // INTERNAL STATE & COMPONENTS
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|MiningLaser|State")
    bool bLaserIsActive;

    // The SceneComponent that represents the laser's origin and orientation.
    // This should be added to the owning actor and then assigned here, or created dynamically.
    // For simplicity, we'll assume it's a child component of the owner that we find by name or tag,
    // or the owner itself if BeamSourceSocketName is not set.
    
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solaraq|MiningLaser|Attachment", meta = (ToolTip = "The SceneComponent on the owning actor that defines the laser's muzzle position and orientation. Resolved at runtime."))
    TObjectPtr<USceneComponent> LaserMuzzleComponent;
    
    UPROPERTY()
    TObjectPtr<UParticleSystemComponent> ActiveBeamCascadePSC; // For Cascade beam

    UPROPERTY()
    TObjectPtr<UNiagaraComponent> ActiveBeamNiagaraComp; // For Niagara beam

    UPROPERTY()
    TObjectPtr<UParticleSystemComponent> ActiveImpactCascadePSC; // For Cascade impact

    UPROPERTY()
    TObjectPtr<UNiagaraComponent> ActiveImpactNiagaraComp; 
    
    UPROPERTY()
    TObjectPtr<UAudioComponent> ActiveLaserAudioComponent;

    FVector CurrentTargetWorldLocation;
    FVector CurrentImpactPoint;
    bool bCurrentlyHittingTarget;

    // FUNCTIONS
    void UpdateLaserAim(float DeltaTime);
    void UpdateLaserBeamVisuals(const FVector& BeamStart, const FVector& BeamEnd, bool bHitSomething);
    void ApplyMiningDamage(float DeltaTime, const FHitResult& HitResult);
    void StartLaserEffects();
    void StopLaserEffects(bool bImmediate = false);
    void UpdateImpactEffect(const FHitResult& HitResult, bool bIsHitting);

public:
    /**
     * Call this from the owning actor (e.g., in BeginPlay or when the component is added)
     * to set the scene component that defines the laser's muzzle position and orientation.
     * If not set, it will try to use the component owner's root or a socket.
     */
    UFUNCTION(BlueprintCallable, Category = "MiningLaser|Attachment")
    void SetLaserMuzzleComponent(USceneComponent* Muzzle);

    UFUNCTION(BlueprintPure, Category = "MiningLaser|Attachment")
    FVector GetLaserMuzzleLocation() const;

    UFUNCTION(BlueprintPure, Category = "MiningLaser|Attachment")
    FRotator GetLaserMuzzleRotation() const;

    UFUNCTION(BlueprintPure, Category = "MiningLaser|Attachment")
    FVector GetLaserMuzzleForwardVector() const;
};