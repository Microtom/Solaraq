// Environment/AsteroidFieldGenerator.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AsteroidFieldGenerator.generated.h" // Always last include for generated files

// Forward declarations - good practice to reduce compile times
class USplineComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

// This is a USTRUCT, which is like a lightweight C++ struct that Unreal's reflection system can understand.
// We'll use this to define what an "asteroid type" is - basically, a mesh and how often it should appear.
USTRUCT(BlueprintType) // BlueprintType means we can use this struct in Blueprints too!
struct FAsteroidTypeDefinition
{
    GENERATED_BODY() // Unreal magic macro

    // This is a TSoftObjectPtr to a UStaticMesh. "Soft" means it doesn't load the mesh immediately,
    // which is great for performance, especially if you have many potential asteroid meshes.
    // We can edit this in the Unreal Editor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Type")
    TSoftObjectPtr<UStaticMesh> Mesh;

    // This is the magic number for variety!
    // Higher weight = this asteroid type is more common.
    // e.g., Mesh A (Weight 10) and Mesh B (Weight 1) means A is 10x more likely to spawn than B.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Type", meta = (ClampMin = "0.01", ToolTip="Higher value means more common."))
    float Weight = 1.0f; // Default weight to 1, so it's always considered if not changed.

    // Constructor for the struct, just setting a default value.
    FAsteroidTypeDefinition() : Weight(1.0f) {}
};

// This is our main Actor class.
UCLASS() // Makes this class visible to Unreal Engine's reflection system.
class SOLARAQ_API AAsteroidFieldGenerator : public AActor
{
    GENERATED_BODY() // More Unreal magic!

public:
    // Constructor
    AAsteroidFieldGenerator();

protected:
    // Called when the game starts or when spawned.
    virtual void BeginPlay() override;
    // This is super handy for editor-time updates! It's called when the actor is constructed in the editor,
    // or when a property is changed if "Run Construction Script on Drag" is true.
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR // This code will only compile in Editor builds
    // Called in the editor when a property of this actor is changed.
    // This allows us to regenerate the asteroid field instantly when we tweak settings!
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    // --- Core Components ---
    // Our root component, everything else will be attached to this.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    USceneComponent* SceneRoot;

    // The spline component defines the path or area for our asteroid field.
    // Think of it like drawing a curve in space.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    USplineComponent* SplineComponent;

    // This array will hold all the HISM components we create.
    // We need one HISM per *unique* static mesh type to get the best performance.
    // UPROPERTY() // We make this a UPROPERTY so it's tracked by the Garbage Collector (GC).
    // Even though we recreate them, it's good practice for TObjectPtr arrays.
    UPROPERTY() // For TObjectPtr arrays to be GC-safe, they need to be UPROPERTY.
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> HISMComponents;


public:
    // --- Asteroid Properties Exposed to Editor ---

    // This is where we'll list all the different asteroid meshes we want to use.
    // It uses our FAsteroidTypeDefinition struct from above.
    // meta = (TitleProperty = "Mesh") makes the editor display the mesh name nicely in the array.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (TitleProperty = "Mesh"))
    TArray<FAsteroidTypeDefinition> AsteroidTypes;

    // How many asteroids do we want?
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (ClampMin = "0"))
    int32 NumberOfInstances;

    // The seed for our random number generator. Using the same seed will always produce the same layout!
    // Great for testing or if you want a specific, repeatable field.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field")
    int32 RandomSeed;

    // Two modes: either a belt along the spline or fill the area enclosed by the spline.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (ToolTip = "If true, fills the area defined by the spline. If false, creates a belt along the spline."))
    bool bFillArea;

    // If not bFillArea, how wide is the belt?
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (EditCondition = "!bFillArea", ClampMin = "0.0"))
    float BeltWidth;

    // If not bFillArea, how tall/thick is the belt?
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (EditCondition = "!bFillArea", ClampMin = "0.0"))
    float BeltHeight;

    // If bFillArea, what's the vertical thickness of the filled area?
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (EditCondition = "bFillArea", ClampMin = "0.0"))
    float FieldHeight;

    // Min/Max scale for the asteroids, for some size variation.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (ClampMin = "0.01"))
    float MinScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (ClampMin = "0.01"))
    float MaxScale;

    // Randomize yaw (left/right rotation)?
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field")
    bool bRandomYaw;

    // Randomize pitch (up/down) and roll (sideways barrel roll)?
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field")
    bool bRandomPitchRoll;

    // This function will do the heavy lifting of actually creating the asteroids.
    // We make it CallInEditor so we can add a button in the Details panel to run it manually!
    UFUNCTION(CallInEditor, Category = "Solaraq|Asteroid Field")
    void GenerateAsteroids();

private:
    // Helper function to get a random point within the belt volume.
    FVector GetRandomPointInBeltVolume(const FRandomStream& Stream) const;
    // Helper function to get a random point within the field volume (if bFillArea is true).
    FVector GetRandomPointInFieldVolume(const FRandomStream& Stream) const;
    // Helper function to calculate the final transform (position, rotation, scale) for an asteroid instance.
    FTransform CalculateInstanceTransform(const FVector& LocalPosition, const FRandomStream& Stream) const;

    // A flag to prevent GenerateAsteroids from running multiple times simultaneously,
    // which can happen with editor events.
    bool bIsGenerating;
};