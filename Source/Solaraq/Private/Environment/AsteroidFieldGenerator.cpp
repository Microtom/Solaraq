// Environment/AsteroidFieldGenerator.cpp

#include "Environment/AsteroidFieldGenerator.h"
#include "Components/SplineComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
// #include "Engine/StaticMesh.h" // Already in .h
#include "Kismet/KismetMathLibrary.h"
#include "Math/RandomStream.h"
#include "Logging/SolaraqLogChannels.h"
#include "UObject/ConstructorHelpers.h" // For MakeUniqueObjectName if not found

AAsteroidFieldGenerator::AAsteroidFieldGenerator()
{
    // ... (SceneRoot and SplineComponent setup remains the same) ...
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
    SplineComponent->SetupAttachment(SceneRoot);
    SplineComponent->SetClosedLoop(true);
    SplineComponent->ClearSplinePoints(false);

    const float DefaultRadius = 10000.0f;
    const FVector P0 = FVector(DefaultRadius, 0.f, 0.f);
    const FVector P1 = FVector(0.f, DefaultRadius, 0.f);
    const FVector P2 = FVector(-DefaultRadius, 0.f, 0.f);
    const FVector P3 = FVector(0.f, -DefaultRadius, 0.f);

    SplineComponent->AddSplinePoint(P0, ESplineCoordinateSpace::Local, false);
    SplineComponent->AddSplinePoint(P1, ESplineCoordinateSpace::Local, false);
    SplineComponent->AddSplinePoint(P2, ESplineCoordinateSpace::Local, false);
    SplineComponent->AddSplinePoint(P3, ESplineCoordinateSpace::Local, false);

    const float TangentMagnitudeFactor = 1.64f;
    const float TangentLength = DefaultRadius * TangentMagnitudeFactor;
    const FVector T0 = FVector(0.f, TangentLength, 0.f);
    const FVector T1 = FVector(-TangentLength, 0.f, 0.f);
    const FVector T2 = FVector(0.f, -TangentLength, 0.f);
    const FVector T3 = FVector(TangentLength, 0.f, 0.f);

    SplineComponent->SetSplinePointType(0, ESplinePointType::Curve, false);
    SplineComponent->SetTangentAtSplinePoint(0, T0, ESplineCoordinateSpace::Local, false);
    SplineComponent->SetSplinePointType(1, ESplinePointType::Curve, false);
    SplineComponent->SetTangentAtSplinePoint(1, T1, ESplineCoordinateSpace::Local, false);
    SplineComponent->SetSplinePointType(2, ESplinePointType::Curve, false);
    SplineComponent->SetTangentAtSplinePoint(2, T2, ESplineCoordinateSpace::Local, false);
    SplineComponent->SetSplinePointType(3, ESplinePointType::Curve, false);
    SplineComponent->SetTangentAtSplinePoint(3, T3, ESplineCoordinateSpace::Local, false);
    SplineComponent->UpdateSpline();


    // Default values
    NumberOfInstances = 100;
    RandomSeed = 12345;
    bFillArea = false;
    BeltWidth = 2000.0f;
    BeltHeight = 500.0f;
    FieldHeight = 1000.0f;
    MinScale = 0.5f;
    MaxScale = 1.5f;
    bRandomYaw = true;
    bRandomPitchRoll = true;
    bIsGenerating = false;
}

void AAsteroidFieldGenerator::BeginPlay()
{
    Super::BeginPlay();
    if (!GetWorld()->IsEditorWorld() && GetWorld()->IsGameWorld())
    {
        // GenerateAsteroids(); // Optionally generate at runtime
    }
}

void AAsteroidFieldGenerator::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GenerateAsteroids();
}

#if WITH_EDITOR
void AAsteroidFieldGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

    if (PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, AsteroidTypes) || // Check new array name
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, NumberOfInstances) ||
        // ... (other properties that should trigger regeneration) ...
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, RandomSeed) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, bFillArea) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, BeltWidth) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, BeltHeight) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, FieldHeight) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, MinScale) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, MaxScale) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, bRandomYaw) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, bRandomPitchRoll))
    {
        GenerateAsteroids();
    }
}
#endif

void AAsteroidFieldGenerator::GenerateAsteroids()
{
    if (bIsGenerating) return;
    bIsGenerating = true;

    if (!SplineComponent)
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("AsteroidFieldGenerator %s: Missing Spline component!"), *GetName());
        bIsGenerating = false;
        return;
    }

    // 1. Clear and Destroy previous HISM components
    for (TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HISM : HISMComponents)
    {
        if (HISM)
        {
            HISM->ClearInstances();
            HISM->UnregisterComponent();
            HISM->DestroyComponent();
        }
    }
    HISMComponents.Empty();

    // 2. Prepare data for generation: Map unique meshes to HISM and gather valid types for weighted selection
    TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> MeshToHISMMap;
    TArray<const FAsteroidTypeDefinition*> ValidSelectableTypes;
    float TotalWeight = 0.0f;

    for (const FAsteroidTypeDefinition& TypeDef : AsteroidTypes)
    {
        if (TypeDef.Mesh.IsNull() || TypeDef.Weight <= 0.0f)
        {
            if (!TypeDef.Mesh.IsNull() && TypeDef.Weight <= 0.0f)
            {
                 UE_LOG(LogSolaraqSystem, Warning, TEXT("AsteroidFieldGenerator %s: AsteroidType with mesh %s has zero or negative weight (%.2f). Skipping."),
                    *GetName(), *TypeDef.Mesh.ToString(), TypeDef.Weight);
            }
            continue; // Skip entries with no mesh or non-positive weight
        }

        TObjectPtr<UStaticMesh> LoadedMesh = TypeDef.Mesh.LoadSynchronous();
        if (!LoadedMesh)
        {
            UE_LOG(LogSolaraqSystem, Warning, TEXT("AsteroidFieldGenerator %s: Failed to load mesh %s. Skipping."), *GetName(), *TypeDef.Mesh.ToString());
            continue;
        }

        // If this mesh doesn't have a HISM yet, create one
        if (!MeshToHISMMap.Contains(LoadedMesh))
        {
            FName HISMName = MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), FName(*FString::Printf(TEXT("AsteroidHISM_%s"), *LoadedMesh->GetName())));
            TObjectPtr<UHierarchicalInstancedStaticMeshComponent> NewHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, HISMName);
            if (NewHISM)
            {
                NewHISM->SetupAttachment(SceneRoot);
                NewHISM->SetStaticMesh(LoadedMesh);
                NewHISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                NewHISM->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
                NewHISM->RegisterComponent();
                
                HISMComponents.Add(NewHISM); // Add to our main list for cleanup
                MeshToHISMMap.Add(LoadedMesh, NewHISM);
                UE_LOG(LogSolaraqSystem, Verbose, TEXT("AsteroidFieldGenerator %s: Created HISM for mesh %s"), *GetName(), *LoadedMesh->GetName());
            }
            else
            {
                UE_LOG(LogSolaraqSystem, Error, TEXT("AsteroidFieldGenerator %s: Failed to create NewHISM for mesh %s."), *GetName(), *LoadedMesh->GetName());
                continue; // Don't process this TypeDef if HISM creation failed
            }
        }
        
        // If we reached here, the mesh is loaded and has a HISM (either new or existing)
        ValidSelectableTypes.Add(&TypeDef); // Store pointer to original TypeDef for weight access
        TotalWeight += TypeDef.Weight;
    }

    if (ValidSelectableTypes.IsEmpty() || TotalWeight <= 0.0f || MeshToHISMMap.IsEmpty())
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("AsteroidFieldGenerator %s: No valid asteroid types to generate from (check meshes and weights). TotalWeight: %.2f"), *GetName(), TotalWeight);
        bIsGenerating = false;
        return;
    }

    // 3. Distribute instances using weighted random selection
    if (NumberOfInstances <= 0)
    {
        bIsGenerating = false;
        return;
    }

    FRandomStream RandomStream(RandomSeed);
    int32 TotalInstancesAdded = 0;

    for (int32 i = 0; i < NumberOfInstances; ++i)
    {
        float RandomPick = RandomStream.FRandRange(0.f, TotalWeight);
        const FAsteroidTypeDefinition* SelectedType = nullptr;
        float CurrentCumulativeWeight = 0.f;

        for (const FAsteroidTypeDefinition* TypePtr : ValidSelectableTypes)
        {
            // Ensure TypePtr is valid before dereferencing (should be, as we added valid pointers)
            if (!TypePtr) continue; 

            if (RandomPick <= CurrentCumulativeWeight + TypePtr->Weight)
            {
                SelectedType = TypePtr;
                break;
            }
            CurrentCumulativeWeight += TypePtr->Weight;
        }

        if (!SelectedType) // Should ideally not happen if TotalWeight > 0
        {
            // Fallback to the first valid type if something went wrong with selection logic
            if (!ValidSelectableTypes.IsEmpty()) SelectedType = ValidSelectableTypes[0];
            else continue; // No types to select from
            UE_LOG(LogSolaraqSystem, Warning, TEXT("AsteroidFieldGenerator %s: Weighted selection failed, falling back to first valid type."), *GetName());
        }
        
        // Get the mesh and corresponding HISM for the selected type
        TObjectPtr<UStaticMesh> MeshForInstance = SelectedType->Mesh.Get(); // Should be loaded
        if (!MeshForInstance) // Double check, or re-load if soft ptrs could unload
        {
            MeshForInstance = SelectedType->Mesh.LoadSynchronous();
            if (!MeshForInstance)
            {
                UE_LOG(LogSolaraqSystem, Error, TEXT("AsteroidFieldGenerator %s: Failed to get/load mesh for selected type %s."), *GetName(), *SelectedType->Mesh.ToString());
                continue;
            }
        }


        const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* FoundHISM = MeshToHISMMap.Find(MeshForInstance);
        if (FoundHISM && *FoundHISM)
        {
            FVector InstanceBasePosition;
            if (bFillArea)
            {
                InstanceBasePosition = GetRandomPointInFieldVolume(RandomStream);
            }
            else
            {
                InstanceBasePosition = GetRandomPointInBeltVolume(RandomStream);
            }

            FTransform InstanceTransform = CalculateInstanceTransform(InstanceBasePosition, RandomStream);
            (*FoundHISM)->AddInstance(InstanceTransform);
            TotalInstancesAdded++;
        }
        else
        {
             UE_LOG(LogSolaraqSystem, Error, TEXT("AsteroidFieldGenerator %s: Could not find HISM for selected mesh %s! Internal error."), *GetName(), *MeshForInstance->GetName());
        }
    }

    UE_LOG(LogSolaraqSystem, Log, TEXT("AsteroidFieldGenerator %s: Generated %d total instances across %d HISM components using weighted selection."), *GetName(), TotalInstancesAdded, MeshToHISMMap.Num());
    bIsGenerating = false;
}

// ... (GetRandomPointInBeltVolume, GetRandomPointInFieldVolume, CalculateInstanceTransform methods remain unchanged) ...
// Paste your existing implementations of these three methods here
FVector AAsteroidFieldGenerator::GetRandomPointInBeltVolume(const FRandomStream& Stream) const
{
	const float SplineLength = SplineComponent->GetSplineLength();
	if (SplineLength < KINDA_SMALL_NUMBER) return FVector::ZeroVector; 

	const float DistanceAlongSpline = Stream.FRandRange(0.0f, SplineLength);
	const FVector PointOnSpline = SplineComponent->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);
	const FVector DirectionOnSpline = SplineComponent->GetDirectionAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);
	const FVector UpVectorOnSpline = SplineComponent->GetUpVectorAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);

	const FVector RightVectorOnSpline = FVector::CrossProduct(DirectionOnSpline, UpVectorOnSpline).GetSafeNormal();

	const float OffsetWidth = Stream.FRandRange(-BeltWidth * 0.5f, BeltWidth * 0.5f);
	const float OffsetHeight = Stream.FRandRange(-BeltHeight * 0.5f, BeltHeight * 0.5f);

	FVector Position = PointOnSpline + (RightVectorOnSpline * OffsetWidth) + (UpVectorOnSpline * OffsetHeight);

	return Position; 
}

FVector AAsteroidFieldGenerator::GetRandomPointInFieldVolume(const FRandomStream& Stream) const
{
	FBox SplineBounds(ForceInit);
	const int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
	if (NumPoints < 2) return FVector::ZeroVector; 

	for (int32 i = 0; i < NumPoints; ++i)
	{
		SplineBounds += SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
	}

	const FVector Center = SplineBounds.GetCenter();
	const float ApproxRadius = SplineBounds.GetExtent().GetMax();

	const float RandomAngle = Stream.FRandRange(0.0f, 2.0f * PI);
	const float RandomRadius = FMath::Sqrt(Stream.FRand()) * ApproxRadius; 
	const float OffsetX = FMath::Cos(RandomAngle) * RandomRadius;
	const float OffsetY = FMath::Sin(RandomAngle) * RandomRadius;
	const float OffsetZ = Stream.FRandRange(-FieldHeight * 0.5f, FieldHeight * 0.5f);

	FVector WorldPosition = FVector(Center.X + OffsetX, Center.Y + OffsetY, Center.Z + OffsetZ);
	FVector LocalPosition = GetTransform().InverseTransformPosition(WorldPosition);

	return LocalPosition;
}

FTransform AAsteroidFieldGenerator::CalculateInstanceTransform(const FVector& LocalPosition, const FRandomStream& Stream) const
{
	const float Scale = Stream.FRandRange(MinScale, MaxScale);
	const FVector Scale3D(Scale);

	FRotator Rotation = FRotator::ZeroRotator;
	if (bRandomYaw)
	{
		Rotation.Yaw = Stream.FRandRange(0.0f, 360.0f);
	}
	if (bRandomPitchRoll)
	{
		Rotation.Pitch = Stream.FRandRange(0.0f, 360.0f);
		Rotation.Roll = Stream.FRandRange(0.0f, 360.0f);
	}
	return FTransform(Rotation, LocalPosition, Scale3D);
}