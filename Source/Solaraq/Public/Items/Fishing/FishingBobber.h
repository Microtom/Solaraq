// FishingBobber.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FishingBobber.generated.h"

// Forward declarations are fine here
class UStaticMeshComponent;
class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class SOLARAQ_API AFishingBobber : public AActor
{
	GENERATED_BODY()
    
public:    
	AFishingBobber(); // FIXED: Added missing constructor declaration

	// --- Components ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// --- Public Functions ---
	void StartFloating(float WaterSurfaceZ);
	void Jiggle();
    
protected:
	virtual void Tick(float DeltaTime) override; // FIXED: Added Tick declaration for buoyancy logic

	// FIXED: Corrected signature to match OnComponentHit delegate
	UFUNCTION()
	void OnBobberHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	// FIXED: Added missing variable declarations used in the .cpp
	bool bIsInWater = false;
	float WaterLevel = 0.0f;
};