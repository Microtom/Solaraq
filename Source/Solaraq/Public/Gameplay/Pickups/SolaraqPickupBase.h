// SolaraqPickupBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SolaraqPickupBase.generated.h" // Make sure this is last

// Forward Declarations
class USphereComponent;
class UStaticMeshComponent;

// Enum to define what kind of pickup this is
UENUM(BlueprintType)
enum class EPickupType : uint8
{
    Resource_Iron   UMETA(DisplayName = "Resource: Iron"),
    Resource_Crystal UMETA(DisplayName = "Resource: Crystal"),
    Ammo_Standard   UMETA(DisplayName = "Ammo: Standard"),
    Health_Pack     UMETA(DisplayName = "Health Pack"),
    // Add more types as needed
};

UCLASS(Blueprintable, BlueprintType) // Allow Blueprint derivation
class SOLARAQ_API ASolaraqPickupBase : public AActor
{
    GENERATED_BODY()

public:
    ASolaraqPickupBase();

protected:
    // --- Components ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> CollisionSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> PickupMesh;

    // --- Properties ---

    // Defines the type of item this pickup represents
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup")
    EPickupType Type = EPickupType::Resource_Iron;

    // How much of the item this pickup gives
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup")
    int32 Quantity = 1;

    // How long the pickup lasts in seconds before destroying itself
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup")
    float LifeSpanSeconds = 30.0f;

    // Strength of the initial push when spawned
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Pickup|Physics") // Allow BP to tweak
    float DispersalImpulseStrength = 500.0f;

    // Damping to make the pickup stop moving
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Pickup|Physics") // Allow BP to tweak
    float LinearDamping = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Pickup|Physics") // Allow BP to tweak
    float AngularDamping = 2.0f;

    // --- Overrides ---

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override; // Optional: For potential future logic

    // Called when an actor overlaps the CollisionSphere
    UFUNCTION() // Needs to be a UFUNCTION to bind to the delegate
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // --- Server-Side Logic ---

    // Applies the initial dispersal impulse (Server Only)
    void ApplyDispersalImpulse();

    // Attempts to give the pickup to the collecting actor (Server Only)
    void TryCollect(class ASolaraqShipBase* CollectingShip);

public:
    // Function to get the pickup type (useful for other classes)
    UFUNCTION(BlueprintPure, Category = "Pickup")
    EPickupType GetPickupType() const { return Type; }

    // Function to get the quantity (useful for other classes)
    UFUNCTION(BlueprintPure, Category = "Pickup")
    int32 GetQuantity() const { return Quantity; }
};