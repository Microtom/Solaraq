// --- START OF FILE AsteroidFieldGenerator.h ---

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AsteroidFieldGenerator.generated.h" // Must be last include

// Forward Declarations
class USplineComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * @brief Actor responsible for procedurally generating asteroid fields or belts within a volume defined by a SplineComponent.
 *
 * Uses a HierarchicalInstancedStaticMeshComponent (HISM) for efficient rendering of a large number of asteroid instances.
 * Supports generation modes:
 *  - Belt: Instances are placed along the spline within a defined width and height.
 *  - Field: Instances fill the approximate area enclosed by a closed spline (best for roughly planar/circular splines) with a defined height.
 * Generation occurs primarily in the editor via OnConstruction and PostEditChangeProperty for immediate feedback,
 * but can be triggered at runtime if needed.
 * Provides parameters for controlling the number of instances, randomization, visual variation (scale, rotation), and shape.
 */
UCLASS(Blueprintable) // Allow creation of Blueprint children if needed
class SOLARAQ_API AAsteroidFieldGenerator : public AActor
{
	GENERATED_BODY()

public:
	/** Default Constructor */
	AAsteroidFieldGenerator();

	//~ Begin AActor Interface
	/** Called when actor is constructed in editor or spawned. Triggers asteroid generation for preview. */
	virtual void OnConstruction(const FTransform& Transform) override;
#if WITH_EDITOR
	/** Called in the editor when a property is changed. Triggers regeneration if relevant properties are modified. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	/** Called when the game starts or when spawned. Can optionally trigger generation if needed at runtime. */
	virtual void BeginPlay() override;
	//~ End AActor Interface

protected:

	// --- Components --------------------------------------------------------
#pragma region Components

	/** Root component for the Actor hierarchy. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Spline defining the path (for belts) or boundary (for fields) of the asteroid generation volume. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> SplineComponent;

	/** Component holding and rendering all the asteroid instances efficiently. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> AsteroidInstances;

#pragma endregion Components

	// --- Generation Parameters ---------------------------------------------
#pragma region Generation Parameters

	/** The static mesh asset to use for the individual asteroid instances. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Parameters", meta = (DisplayName = "Asteroid Mesh"))
	TSoftObjectPtr<UStaticMesh> AsteroidMesh;

	/** Total number of asteroid instances to attempt generating within the defined volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Parameters", meta = (ClampMin = "0", UIMin = "0"))
	int32 NumberOfInstances = 1000;

	/** Seed used for the random number generator to ensure deterministic placement if desired. Change seed for different layouts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Parameters")
	int32 RandomSeed = 1234;

	/**
	 * @brief Determines the generation shape.
	 * If true: Fills the approximate area enclosed by the SplineComponent (best for closed, planar shapes).
	 * If false: Generates instances in a belt shape along the SplineComponent path.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Parameters", meta = (DisplayName = "Fill Area (Field Mode)"))
	bool bFillArea = false;

#pragma endregion Generation Parameters

	// --- Belt Parameters ---------------------------------------------------
#pragma region Belt Parameters

	/** Width of the generation volume perpendicular to the spline direction (total width, centered on spline). Used only if bFillArea is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Belt", meta = (EditCondition = "!bFillArea", ForceUnits="cm", ClampMin = "0.0"))
	float BeltWidth = 2000.0f;

	/** Height of the generation volume above and below the spline (total height, centered on spline). Used only if bFillArea is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Belt", meta = (EditCondition = "!bFillArea", ForceUnits="cm", ClampMin = "0.0"))
	float BeltHeight = 500.0f;

#pragma endregion Belt Parameters

	// --- Field Parameters --------------------------------------------------
#pragma region Field Parameters

	/** Approximate vertical thickness of the generated field volume when bFillArea is true. Centered vertically on spline bounds center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Field", meta = (EditCondition = "bFillArea", ForceUnits="cm", ClampMin = "0.0"))
	float FieldHeight = 1000.0f;

#pragma endregion Field Parameters

	// --- Instance Variation ------------------------------------------------
#pragma region Instance Variation

	/** Minimum scale multiplier randomly applied to each asteroid instance (relative to base mesh scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Variation", meta = (ClampMin = "0.01"))
	float MinScale = 0.5f;

	/** Maximum scale multiplier randomly applied to each asteroid instance (relative to base mesh scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Variation", meta = (ClampMin = "0.01"))
	float MaxScale = 2.0f;

	/** If true, applies a random rotation around the Z-axis (Yaw) to each instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Variation")
	bool bRandomYaw = true;

	/** If true, applies random rotations around the X (Roll) and Y (Pitch) axes to each instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Variation")
	bool bRandomPitchRoll = true;

#pragma endregion Instance Variation

	// --- Generation Logic --------------------------------------------------
#pragma region Generation Logic

	/**
	 * @brief Main function to clear existing instances and generate new ones based on current parameters.
	 * Called by OnConstruction, PostEditChangeProperty, and optionally BeginPlay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Generation|Logic")
	void GenerateAsteroids();

#pragma endregion Generation Logic

private:
	// --- Internal Helpers --------------------------------------------------

	/**
	 * @brief Calculates a random position within the belt volume defined by the spline and BeltWidth/BeltHeight parameters.
	 * @param Stream The random number stream to use for generation.
	 * @return A random position in Actor-Local space.
	 */
	FVector GetRandomPointInBeltVolume(const FRandomStream& Stream) const;

	/**
	 * @brief Calculates a random position within the field volume approximated by the spline bounds and FieldHeight.
	 * @param Stream The random number stream to use for generation.
	 * @return A random position in Actor-Local space.
	 */
	FVector GetRandomPointInFieldVolume(const FRandomStream& Stream) const;

	/**
	 * @brief Calculates the final local transform (Location, Rotation, Scale) for a single asteroid instance.
	 * @param LocalPosition The base local position calculated by GetRandomPointInBeltVolume or GetRandomPointInFieldVolume.
	 * @param Stream The random number stream to use for variation.
	 * @return The final FTransform for the instance relative to the Actor's origin.
	 */
	FTransform CalculateInstanceTransform(const FVector& LocalPosition, const FRandomStream& Stream) const;

	/** Internal flag to prevent unintended recursive calls to GenerateAsteroids, e.g., during editor property updates. */
	bool bIsGenerating = false;
};

// --- END OF FILE AsteroidFieldGenerator.h ---