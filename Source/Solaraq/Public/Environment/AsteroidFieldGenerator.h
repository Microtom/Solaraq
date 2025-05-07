// Environment/AsteroidFieldGenerator.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AsteroidFieldGenerator.generated.h"

class USplineComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

USTRUCT(BlueprintType)
struct FAsteroidTypeDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Type")
    TSoftObjectPtr<UStaticMesh> Mesh;

    // Relative probability weight for this mesh type.
    // e.g., Mesh A with Weight 10 and Mesh B with Weight 3 means Mesh A is roughly 10/13 likely
    // and Mesh B is roughly 3/13 likely to be chosen over one another.
    // Higher values mean more common.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Type", meta = (ClampMin = "0.01", ToolTip="Higher value means more common."))
    float Weight = 1.0f;

    FAsteroidTypeDefinition() : Weight(1.0f) {}
};

UCLASS()
class SOLARAQ_API AAsteroidFieldGenerator : public AActor
{
    GENERATED_BODY()

public:
    AAsteroidFieldGenerator();

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    USceneComponent* SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    USplineComponent* SplineComponent;

    // Array to hold references to the dynamically created HISM components
    // UPROPERTY() // Make UPROPERTY so they are tracked by GC and potentially serialized (though we recreate them)
    TArray<UHierarchicalInstancedStaticMeshComponent*> HISMComponents; // Changed name

public:
    // --- Asteroid Properties ---

    // Array of meshes to choose from for asteroids
    // Defines the types of asteroid meshes and their relative spawn weights.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (TitleProperty = "Mesh"))
    TArray<FAsteroidTypeDefinition> AsteroidTypes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (ClampMin = "0"))
    int32 NumberOfInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field")
    int32 RandomSeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (ToolTip = "If true, fills the area defined by the spline. If false, creates a belt along the spline."))
    bool bFillArea;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (EditCondition = "!bFillArea", ClampMin = "0.0"))
    float BeltWidth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (EditCondition = "!bFillArea", ClampMin = "0.0"))
    float BeltHeight; // Vertical thickness of the belt

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (EditCondition = "bFillArea", ClampMin = "0.0"))
    float FieldHeight; // Vertical thickness of the filled area

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (ClampMin = "0.01"))
    float MinScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field", meta = (ClampMin = "0.01"))
    float MaxScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field")
    bool bRandomYaw;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Field")
    bool bRandomPitchRoll;

    UFUNCTION(CallInEditor, Category = "Solaraq|Asteroid Field") // Allow calling from editor button
    void GenerateAsteroids();

private:
    FVector GetRandomPointInBeltVolume(const FRandomStream& Stream) const;
    FVector GetRandomPointInFieldVolume(const FRandomStream& Stream) const;
    FTransform CalculateInstanceTransform(const FVector& LocalPosition, const FRandomStream& Stream) const;

    bool bIsGenerating; // To prevent recursive calls
};