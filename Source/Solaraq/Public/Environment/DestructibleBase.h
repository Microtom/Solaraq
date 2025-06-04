#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DestructibleBase.generated.h"

// Forward declarations
class UGeometryCollectionComponent;
class UParticleSystem;
class USoundBase;
struct FChaosBreakEvent; // Required for the delegate parameter

UCLASS(Abstract) // Abstract makes it so this base class cannot be placed directly in the world, only derived classes.
class SOLARAQ_API ADestructibleBase : public AActor
{
    GENERATED_BODY()

public:
    ADestructibleBase();

protected:
    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
    //~ End AActor Interface

    // COMPONENTS
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComponent;

    // PROPERTIES
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Health", meta = (ClampMin = "0.0"))
    float MaxHealth;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Destruction|Health")
    float CurrentHealth;

    // Minimum damage from a single hit to be considered significant enough to potentially trigger destruction,
    // even if health isn't fully depleted. Can also be used to ensure small hits don't instantly shatter.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Thresholds", meta = (ClampMin = "0.0"))
    float MinSignificantDamageToFracture;

    UPROPERTY(BlueprintReadOnly, Category = "Destruction")
    bool bIsDestroyed;

    // EFFECTS
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Effects")
    TObjectPtr<UParticleSystem> DestructionParticleSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Effects")
    TObjectPtr<USoundBase> DestructionSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Effects")
    TObjectPtr<UParticleSystem> PieceBrokenParticleSystem; // Optional: effect for when individual pieces break

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Effects")
    TObjectPtr<USoundBase> PieceBrokenSound; // Optional: sound for when individual pieces break


    // EVENT HANDLERS & LOGIC
    /**
     * Called when the Geometry Collection component registers a break.
     * This can happen for individual pieces breaking off.
     */
    UFUNCTION()
    virtual void HandleChaosBreakEvent(const FChaosBreakEvent& BreakEvent);

    /**
     * Primary function to call when the object is considered fully destroyed
     * (e.g., health depleted or catastrophic damage).
     * Handles effects, state changes, and calls Blueprint events.
     */
    virtual void PerformFullDestruction(AActor* DamageCauser);

    /**
     * Blueprint native event called when the object is fully destroyed.
     * Implement custom logic in C++ (_Implementation) or Blueprint.
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Destruction")
    void OnFullyDestroyed(AActor* DamageCauser);
    virtual void OnFullyDestroyed_Implementation(AActor* DamageCauser);

    /**
     * Blueprint native event called when a piece of the destructible breaks off.
     * Provides location and impulse direction for context.
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Destruction")
    void OnPieceBroken(const FVector& PieceLocation, const FVector& PieceImpulseDir);
    virtual void OnPieceBroken_Implementation(const FVector& PieceLocation, const FVector& PieceImpulseDir);


public:
    UFUNCTION(BlueprintPure, Category = "Destruction|Health")
    float GetCurrentHealth() const { return CurrentHealth; }

    UFUNCTION(BlueprintPure, Category = "Destruction|Health")
    float GetMaxHealth() const { return MaxHealth; }

    UFUNCTION(BlueprintPure, Category = "Destruction")
    bool IsDestroyed() const { return bIsDestroyed; }

    /**
     * Allows external systems or damage events to directly trigger the full destruction sequence.
     * Useful if destruction isn't solely health-based (e.g., a specific "destroy" command).
     */
    UFUNCTION(BlueprintCallable, Category = "Destruction")
    virtual void TriggerFullDestruction(AActor* DamageCauser = nullptr);
};