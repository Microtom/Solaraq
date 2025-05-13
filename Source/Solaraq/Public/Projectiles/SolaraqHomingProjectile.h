#pragma once

#include "CoreMinimal.h"
#include "Projectiles/SolaraqProjectile.h"
#include "SolaraqHomingProjectile.generated.h"

class UProjectileMovementComponent;

UCLASS()
class SOLARAQ_API ASolaraqHomingProjectile : public ASolaraqProjectile // Replace YOURPROJECT_API
{
	GENERATED_BODY()

public:
	ASolaraqHomingProjectile();

	virtual void Tick(float DeltaTime) override;

	void SetupHomingTarget(AActor* NewTarget);

	// How quickly the missile can adjust its course (higher is faster turning)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Homing")
	float HomingAccelerationMagnitude;

protected:
	virtual void BeginPlay() override;
    
	UPROPERTY(ReplicatedUsing = OnRep_TargetActor, VisibleInstanceOnly, BlueprintReadOnly, Category = "Homing")
	TWeakObjectPtr<AActor> TargetActor;

	UFUNCTION()
	void OnRep_TargetActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// Cached for efficiency, set in BeginPlay if needed, or ensure ProjectileMovement is accessible
	// UPROPERTY() TObjectPtr<UProjectileMovementComponent> HomingMovementComponent; // Already have ProjectileMovement from base
};