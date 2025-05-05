// --- START OF FILE SolaraqSatellite.h ---

// SolaraqSatellite.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SolaraqSatellite.generated.h" // Must be last include

// Forward Declarations
class UStaticMeshComponent;
class ACelestialBodyBase;

/**
 * @brief Represents a satellite Actor that orbits a specified CelestialBodyBase.
 *
 * The orbit is calculated on a 2D plane defined by GameplayPlaneZ.
 * Movement is server-authoritative and replicated to clients using standard Actor movement replication.
 * Requires CelestialBodyToOrbit to be assigned in the editor instance properties for orbit functionality.
 */
UCLASS()
class SOLARAQ_API ASolaraqSatellite : public AActor
{
	GENERATED_BODY()

public:
	/** Constructor */
	ASolaraqSatellite();

	//~ Begin AActor Interface
	/** Called every frame if ticking. Updates orbit position on the server. */
	virtual void Tick(float DeltaTime) override;
	/** Called when the game starts or when spawned. Initializes orbit on the server if possible. */
	virtual void BeginPlay() override;
	/** Returns properties that are replicated for the lifetime of the actor channel */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#if WITH_EDITOR
	/** Called when actor is constructed or properties changed in editor. Updates visual position in editor. */
	virtual void OnConstruction(const FTransform& Transform) override;
#endif
	//~ End AActor Interface

	//~ Begin UObject Interface
#if WITH_EDITOR
	/** Called in the editor when a property is changed in the details panel. Updates visual position in editor. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

protected:
	// --- Components ---

	/** The visual mesh for the satellite. Also serves as the RootComponent. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SatelliteMeshComponent;

	// --- Orbit Parameters ---

	/** The Celestial Body this satellite orbits around. Must be set in the level instance. Replicated. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Orbit", ReplicatedUsing = OnRep_CelestialBodyToOrbit)
	TObjectPtr<ACelestialBodyBase> CelestialBodyToOrbit;

	/** The distance from the projected center of the celestial body on the gameplay plane. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Orbit", meta = (UIMin = "0.0", ClampMin = "0.0", ForceUnits="cm"))
	float OrbitDistance = 5000.0f;

	/** Speed of orbit in degrees per second. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Orbit")
	float OrbitSpeed = 10.0f;

	/** If true, orbits clockwise when viewed from above, otherwise counter-clockwise. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Orbit")
	bool bClockwiseOrbit = true;

	/** The Z height of the main gameplay plane. Satellite orbits will be projected onto this plane. Usually 0. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit")
	float GameplayPlaneZ = 0.0f;

	// --- Internal State ---

	/** The current angle in the orbit (degrees, 0-360). Managed internally on server. Not replicated directly. */
	UPROPERTY(Transient)
	float CurrentOrbitAngle = 0.0f;

	/** Caches the projected center location of the orbited body to avoid recalculating if the body hasn't moved */
	UPROPERTY(Transient)
	FVector CachedProjectedCenter = FVector::ZeroVector;

	/** Flag to force recalculation of projected center on the next update. */
	UPROPERTY(Transient)
	bool bRecalculateProjectedCenter = true;

	// --- Replication ---

	/** Replication notification function for CelestialBodyToOrbit. Called on Clients. */
	UFUNCTION()
	virtual void OnRep_CelestialBodyToOrbit();

	// --- Orbit Logic ---

	/** Server-side function to calculate and apply the new orbit position based on DeltaTime. */
	void UpdateOrbitPosition(float DeltaTime);

	// --- Editor Helpers ---
#if WITH_EDITOR
	/** Helper function to update the actor's position in the editor viewport based on current orbit parameters. */
	void UpdatePositionInEditor();
#endif

};

// --- END OF FILE SolaraqSatellite.h ---