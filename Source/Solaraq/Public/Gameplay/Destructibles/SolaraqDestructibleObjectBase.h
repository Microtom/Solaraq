// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.h" // For team affiliation
#include "Chaos/ChaosGameplayEventDispatcher.h" // For FChaosPhysicsCollisionInfo
#include "SolaraqDestructibleObjectBase.generated.h"

class UGeometryCollectionComponent; // Replaced UStaticMeshComponent
class UParticleSystem;
class USoundCue;
class USolaraqGimbalGunComponent;

UCLASS()
class SOLARAQ_API ASolaraqDestructibleObjectBase : public AActor, public IGenericTeamAgentInterface
{
    GENERATED_BODY()

public:
    ASolaraqDestructibleObjectBase();

    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    //~ End AActor Interface

    //~ Begin IGenericTeamAgentInterface
    virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }
    // virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override; // Implement if needed
    //~ End IGenericTeamAgentInterface

    UFUNCTION(BlueprintCallable, Category = "Health")
    float GetHealthPercentage() const;

    UFUNCTION(BlueprintPure, Category = "Health")
    bool IsDestroyed() const { return bIsDestroyed_Internal; }

protected:
    // Main visual and physics component for Chaos Destruction
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UGeometryCollectionComponent* GeometryCollectionComponent;

    // Optional Gimbal Gun Component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (EditCondition = "bCanHostGimbalGun", EditConditionHides))
    USolaraqGimbalGunComponent* GimbalGunComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bCanHostGimbalGun = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Health")
    float MaxHealth = 100.0f;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentHealth, BlueprintReadOnly, Category = "Config|Health")
    float CurrentHealth_Internal;

    UPROPERTY(ReplicatedUsing = OnRep_IsDestroyed, BlueprintReadOnly, Category = "State")
    bool bIsDestroyed_Internal = false;

    UFUNCTION()
    void OnRep_CurrentHealth();

    UFUNCTION()
    void OnRep_IsDestroyed();

    // --- Destruction Properties ---
    UPROPERTY(EditDefaultsOnly, Category = "Destruction|Chaos")
    float ChaosDestructionImpulseStrength = 500000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Destruction|Chaos")
    float ChaosDestructionImpulseRadius = 1000.0f;

    // Main explosion effect when health reaches zero (triggers Chaos)
    UPROPERTY(EditDefaultsOnly, Category = "Destruction|Effects")
    UParticleSystem* DestructionEffect; // Big boom particle

    UPROPERTY(EditDefaultsOnly, Category = "Destruction|Effects")
    USoundCue* DestructionSound; // Big boom sound

    // Effect spawned at each individual Chaos chunk break point
    UPROPERTY(EditDefaultsOnly, Category = "Destruction|Effects")
    UParticleSystem* ChunkBreakEffect; // Small dust/spark particle

    UPROPERTY(EditDefaultsOnly, Category = "Destruction|Effects")
    USoundCue* ChunkBreakSound; // Small crack/break sound

    UPROPERTY(EditDefaultsOnly, Category = "Destruction")
    float TimeToDestroyActorAfterChaos = 10.0f; // Time before the main actor itself is cleaned up. GC pieces live on their own.

    // --- Team Affiliation ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
    FGenericTeamId TeamId = FGenericTeamId(2); // Example: Team 2 for neutral/environment

    // Server function to handle the actual destruction logic
    virtual void HandleDestruction(AActor* DamageCauser, const FDamageEvent& InstigatingDamageEvent);

    // Multicast function to play general destruction effects (main explosion)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayMainDestructionEffects();

    // Called when a Chaos geometry collection piece breaks. Can be used for secondary effects.
    UFUNCTION()
    virtual void OnChaosPhysicsBreak(const FChaosBreakEvent& BreakEvent); 

    // Called when the object mesh is hit by something (if collision generates hit events)
    // This is for physics impacts, not direct weapon damage.
    UFUNCTION()
    virtual void OnGeometryCollectionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};