// SolaraqProjectile.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SolaraqProjectile.generated.h" // Must be last include

// Forward Declarations
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UDamageType;

UCLASS(Config=Game, Blueprintable, BlueprintType) // Allow Blueprint children and config variables
class SOLARAQ_API ASolaraqProjectile : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    ASolaraqProjectile();

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // --- Components ---

    /** Sphere collision component - Primary collision and physics interaction */
    UPROPERTY(VisibleDefaultsOnly, Category = "Projectile")
    TObjectPtr<USphereComponent> CollisionComp;

    /** Static Mesh component for visual representation */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ProjectileMesh;

    /** Projectile movement component - Handles velocity, gravity, bouncing etc. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

    // --- Properties ---

    /** Base damage dealt by this projectile on hit */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
    float BaseDamage = 25.0f;

    /** Type of damage this projectile deals */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
    TSubclassOf<UDamageType> DamageTypeClass;

    /** Lifespan of the projectile in seconds. 0 = infinite. */
    UPROPERTY(EditDefaultsOnly, Category = "Projectile")
    float ProjectileLifeSpan = 5.0f;

    // --- Collision Handling ---

    /** Function called when this projectile hits something */
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:
    // --- Accessors ---

    /** Returns CollisionComp subobject **/
    USphereComponent* GetCollisionComp() const { return CollisionComp; }
    /** Returns ProjectileMovement subobject **/
    UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }
};