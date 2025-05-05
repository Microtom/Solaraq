// --- START OF FILE CelestialBodyBase.h ---

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CelestialBodyBase.generated.h"

// Forward Declarations
class UStaticMeshComponent;
class USphereComponent;
class ASolaraqShipBase;

/**
 * @brief Abstract base class for large celestial objects (Planets, Stars, Moons).
 *
 * Provides functionality for:
 * - Exerting a gravitational pull on nearby physics objects (specifically Solaraq Ships).
 * - Applying visual scaling effects to ships as they approach.
 * Uses two main spheres:
 *   - InfluenceSphereComponent: Defines the outer boundary for gravity and general interaction.
 *   - ScalingSphereComponent: Defines the boundary where ship visual scaling begins.
 * Requires derived Blueprints for specific visuals and parameter tuning.
 * Gravity and Scaling logic is primarily Server-Authoritative.
 */
UCLASS(Abstract, Blueprintable) // Abstract: Cannot place directly. Blueprintable: Can create BP children.
class SOLARAQ_API ACelestialBodyBase : public AActor
{
	GENERATED_BODY()

public:
	/** Constructor */
	ACelestialBodyBase();

	//~ Begin AActor Interface
	/** Called every frame if ticked is enabled. */
	virtual void Tick(float DeltaTime) override;
	/** Called when actor is constructed or properties changed in editor. Updates component radii. */
	virtual void OnConstruction(const FTransform& Transform) override;
	/** Called when the game starts or when spawned. Caches radii and validates settings. */
	virtual void BeginPlay() override;
	//~ End AActor Interface

	//~ Begin UObject Interface
#if WITH_EDITOR
	/** Called in the editor when a property is changed in the details panel. Validates and updates radii. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

protected:
	// --- Components ---

	/** Flexible root component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Visual representation of the celestial body (Planet/Star model). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMeshComponent;

	/** Sphere defining the outer boundary of gravitational influence and the limit of scaling effects. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> InfluenceSphereComponent;

	/** Sphere defining the distance from the center where ship visual scaling begins. Does not trigger overlaps itself. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "Scaling Start Sphere"))
	TObjectPtr<USphereComponent> ScalingSphereComponent;

	// --- Influence Parameters ---

	/** The radius of the sphere defining gravitational influence and the absolute maximum range of scaling effects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Body|Influence", meta = (DisplayName = "Influence Radius", ForceUnits="cm"))
	float InfluenceRadius = 15000.0f;

	/** The radius of the sphere where ship scaling begins. Must be <= Influence Radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Body|Influence", meta = (DisplayName = "Scaling Start Radius", ClampMin = "0.0", ForceUnits="cm"))
	float ScalingRadius = 4000.0f;

	// --- Gravity Parameters ---

	/** Arbitrary strength factor for the gravitational pull. Not based on real physics mass. Needs tuning per body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Body|Gravity")
	float GravitationalStrength = 100000.0f;

	/** Exponent controlling how quickly gravity weakens with distance relative to InfluenceRadius. 1.0=linear, 2.0=inverse square (approx). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Body|Gravity", meta = (ClampMin = "0.1"))
	float GravityFalloffExponent = 1.5f;

	// --- Scaling Parameters ---

	/** Distance from the center at which a ship reaches its minimum scale factor. Should be slightly larger than the visual mesh radius to avoid clipping. Must be <= ScalingRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Body|Scaling", meta = (ClampMin = "0.0", ForceUnits="cm"))
	float MinScaleDistance = 200.0f;

	/** The minimum scale factor applied to the ship's visual mesh (e.g., 0.1 = 10% of original size). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Body|Scaling", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float MinShipScaleFactor = 0.1f;

	// --- Internal Logic ---

	/** Called when an actor enters the main InfluenceSphereComponent. Adds ships to AffectedShips list. */
	UFUNCTION()
	void OnInfluenceOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Called when an actor leaves the main InfluenceSphereComponent. Removes ships from AffectedShips and resets their scale. */
	UFUNCTION()
	void OnInfluenceOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Server-side function to apply gravity force and calculate target scale for a specific ship within influence. */
	void ApplyEffectsToShip(ASolaraqShipBase* Ship, float DeltaTime);

	/** Array to keep track of ships currently within the influence sphere (Server-side). */
	UPROPERTY(Transient) // Transient as it's populated at runtime
	TArray<TObjectPtr<ASolaraqShipBase>> AffectedShips;

	/** Helper function to calculate the desired visual scale factor for a ship based on its distance. */
	float CalculateShipScaleFactor(float Distance) const;

	/** Helper function to calculate the gravity force vector acting on a ship based on its distance and location. */
	FVector CalculateGravityForce(float Distance, const FVector& ShipLocation) const;

	/** Cached influence sphere radius (accounts for actor scale) for performance. Updated by UpdateInfluenceSphereRadius. */
	UPROPERTY(Transient)
	float MaxInfluenceDistance = 0.0f;

	/** Cached scaling sphere radius (accounts for actor scale) for performance. Updated by UpdateScalingSphereRadius. */
	UPROPERTY(Transient)
	float MaxScalingDistance = 0.0f;

	// --- Helper Functions ---

	/** Updates the InfluenceSphereComponent's radius based on the InfluenceRadius property and caches MaxInfluenceDistance. */
	void UpdateInfluenceSphereRadius();

	/** Updates the ScalingSphereComponent's radius based on the ScalingRadius property and caches MaxScalingDistance. */
	void UpdateScalingSphereRadius();

	/** Ensures radii properties are valid (non-negative) and ScalingRadius <= InfluenceRadius. Called during construction and property changes. */
	void ValidateRadii();
};

// --- END OF FILE CelestialBodyBase.h ---