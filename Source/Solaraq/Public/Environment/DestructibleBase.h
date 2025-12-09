#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DestructibleBase.generated.h"

// Forward declarations
class UGeometryCollectionComponent;
class UParticleSystem;
class USoundBase;
struct FChaosBreakEvent; 

UCLASS(Abstract)
class SOLARAQ_API ADestructibleBase : public AActor
{
    GENERATED_BODY()

public:
    ADestructibleBase();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    // COMPONENTS
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComponent;

    // PROPERTIES
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Health", meta = (ClampMin = "0.0"))
    float MaxHealth;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Destruction|Health")
    float CurrentHealth;

    UPROPERTY(BlueprintReadOnly, Category = "Destruction")
    bool bIsDestroyed;

    // --- RESTORED PROPERTY ---
    /** Minimum damage required to trigger partial fractures (if using strain logic). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Thresholds", meta = (ClampMin = "0.0"))
    float MinSignificantDamageToFracture;

    // --- NEW CONFIGURATION PROPERTIES ---
    
    /** Strength of the explosion when fully destroyed. Lower = slower debris. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Physics")
    float ExplosionImpulseStrength; 

    /** How long (in seconds) debris exists before being fully removed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Cleanup")
    float DebrisLifeSpan;

    /*For the asteroids to actually "Fade" visually, your Asteroid Material needs a Scalar Parameter
     *named Dissolve or Opacity connected to the Opacity/Opacity Mask slot. If your material is standard Opaque,
     *they won't fade, they will just disappear at 20 seconds (which is still fine).*/
    
    /** How long the fade-out transition lasts at the end of the lifespan. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Cleanup")
    float DebrisFadeDuration;

    // ------------------------------------

    // EFFECTS
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Effects")
    TObjectPtr<UParticleSystem> DestructionParticleSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Effects")
    TObjectPtr<USoundBase> DestructionSound;

    // --- RESTORED PROPERTIES ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Effects")
    TObjectPtr<UParticleSystem> PieceBrokenParticleSystem; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Effects")
    TObjectPtr<USoundBase> PieceBrokenSound;
    // ---------------------------

    // EVENT HANDLERS
    UFUNCTION()
    virtual void HandleChaosBreakEvent(const FChaosBreakEvent& BreakEvent);

    virtual void PerformFullDestruction(AActor* DamageCauser);

    UFUNCTION(BlueprintNativeEvent, Category = "Destruction")
    void OnFullyDestroyed(AActor* DamageCauser);
    virtual void OnFullyDestroyed_Implementation(AActor* DamageCauser);

    UFUNCTION(BlueprintNativeEvent, Category = "Destruction")
    void OnPieceBroken(const FVector& PieceLocation, const FVector& PieceImpulseDir);
    virtual void OnPieceBroken_Implementation(const FVector& PieceLocation, const FVector& PieceImpulseDir);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_Shatter(); 

public:
    UFUNCTION(BlueprintPure, Category = "Destruction")
    bool IsDestroyed() const { return bIsDestroyed; }

    UFUNCTION(BlueprintCallable, Category = "Destruction")
    virtual void TriggerFullDestruction(AActor* DamageCauser = nullptr);

private:
    // Internal state for fading
    bool bIsFadingOut;
    float TimeSinceFadeStarted;
    FTimerHandle TimerHandle_StartFade;

    void StartFadingOut();
};