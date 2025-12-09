// Environment/AsteroidFieldGenerator.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AsteroidFieldGenerator.generated.h"

class USplineComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

// STRUCT FOR VISUAL ASTEROIDS
USTRUCT(BlueprintType)
struct FAsteroidTypeDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Type")
    TSoftObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Type", meta = (ClampMin = "0.01", ToolTip="Higher value means more common."))
    float Weight = 1.0f;
};

// STRUCT FOR MINABLE/INTERACTABLE ASTEROIDS
USTRUCT(BlueprintType)
struct FInteractableAsteroidDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Type")
    TSubclassOf<AActor> ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Asteroid Type", meta = (ClampMin = "0.01"))
    float Weight = 1.0f;
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

    UPROPERTY() 
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> HISMComponents;
    
    UPROPERTY()
    TArray<TObjectPtr<AActor>> SpawnedInteractables;

public:
    // --- SPLINE SETTINGS (NEW) ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Spline Settings")
    bool bAutoRebuildSpline;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Spline Settings", meta = (EditCondition = "bAutoRebuildSpline"))
    float FieldRadius;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Spline Settings", meta = (EditCondition = "bAutoRebuildSpline", ToolTip="Controls the curve. For a perfect circle, this is roughly Radius * 0.552"))
    float TangentHandleLength;

    UFUNCTION(CallInEditor, Category = "Solaraq|Spline Settings")
    void RebuildSpline();

    // --- VISUAL (Background) Asteroid Properties ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Visual Field", meta = (TitleProperty = "Mesh"))
    TArray<FAsteroidTypeDefinition> AsteroidTypes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Visual Field", meta = (ClampMin = "0"))
    int32 NumberOfInstances;

    // --- INTERACTABLE (Gameplay) Asteroid Properties ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Interactable Field", meta = (TitleProperty = "ActorClass"))
    TArray<FInteractableAsteroidDef> InteractableTypes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Interactable Field", meta = (ClampMin = "0"))
    int32 NumberOfInteractables;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Interactable Field")
    bool bRestrictInteractablesToPlane;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Interactable Field", meta = (EditCondition="bRestrictInteractablesToPlane"))
    float GameplayPlaneOffsetZ;

    // --- General Properties ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|General")
    int32 RandomSeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|General")
    bool bFillArea;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|General", meta = (EditCondition = "!bFillArea"))
    float BeltWidth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|General", meta = (EditCondition = "!bFillArea"))
    float BeltHeight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|General", meta = (EditCondition = "bFillArea"))
    float FieldHeight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|General")
    float MinScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|General")
    float MaxScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|General")
    bool bRandomYaw;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|General")
    bool bRandomPitchRoll;

    UFUNCTION(CallInEditor, Category = "Solaraq|General")
    void GenerateAsteroids();

private:
    FVector GetRandomPointInBeltVolume(const FRandomStream& Stream) const;
    FVector GetRandomPointInFieldVolume(const FRandomStream& Stream) const;
    FTransform CalculateInstanceTransform(const FVector& LocalPosition, const FRandomStream& Stream) const;

    bool bIsGenerating;
};