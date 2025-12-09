// Environment/AsteroidFieldGenerator.cpp

#include "Environment/AsteroidFieldGenerator.h"
#include "Components/SplineComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Math/RandomStream.h"
#include "Logging/SolaraqLogChannels.h"
#include "UObject/ConstructorHelpers.h"

AAsteroidFieldGenerator::AAsteroidFieldGenerator()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
    SplineComponent->SetupAttachment(SceneRoot);
    SplineComponent->SetClosedLoop(true);
    // Note: We don't force points here in the constructor anymore. 
    // We rely on OnConstruction to build the initial circle based on the UPROPERTIES.

    // Spline Defaults
    bAutoRebuildSpline = true;
    FieldRadius = 1500.0f;
    // Magic number for a Bezier circle approximation is ~0.55228 * Radius
    TangentHandleLength = 1500.0f * 0.55228f; 

    // Generator Defaults
    NumberOfInstances = 100;
    NumberOfInteractables = 10;
    bRestrictInteractablesToPlane = true;
    GameplayPlaneOffsetZ = 0.0f;
    
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
}

void AAsteroidFieldGenerator::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // If enabled, strictly enforce the circular shape defined by Radius/Tangent
    // This allows you to resize it easily in details, but you should uncheck it
    // if you want to manually move spline points.
    if (bAutoRebuildSpline)
    {
        RebuildSpline();
    }

    GenerateAsteroids();
}

#if WITH_EDITOR
void AAsteroidFieldGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    // Auto-update tangent length if radius changes (optional QOL)
    if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, FieldRadius))
    {
        if (bAutoRebuildSpline)
        {
            TangentHandleLength = FieldRadius * 0.55228f;
        }
    }

    Super::PostEditChangeProperty(PropertyChangedEvent);
    
    // GenerateAsteroids is called via OnConstruction, but sometimes strictly editor-only changes 
    // need an explicit call if OnConstruction doesn't fire for that specific property type.
    // Usually OnConstruction is enough.
}
#endif

void AAsteroidFieldGenerator::RebuildSpline()
{
    if (!SplineComponent) return;

    SplineComponent->ClearSplinePoints(false);

    // 4-Point Circle Calculation
    // P0: (R, 0)
    // P1: (0, R)
    // P2: (-R, 0)
    // P3: (0, -R)
    
    // For Counter-Clockwise rotation:
    // P0 Tangent points +Y
    // P1 Tangent points -X
    // P2 Tangent points -Y
    // P3 Tangent points +X

    const float R = FieldRadius;
    const float L = TangentHandleLength;

    // Point 0 (Right)
    SplineComponent->AddSplinePoint(FVector(R, 0.f, 0.f), ESplineCoordinateSpace::Local, false);
    SplineComponent->SetTangentsAtSplinePoint(0, FVector(0.f, L, 0.f), FVector(0.f, L, 0.f), ESplineCoordinateSpace::Local, false);

    // Point 1 (Forward)
    SplineComponent->AddSplinePoint(FVector(0.f, R, 0.f), ESplineCoordinateSpace::Local, false);
    SplineComponent->SetTangentsAtSplinePoint(1, FVector(-L, 0.f, 0.f), FVector(-L, 0.f, 0.f), ESplineCoordinateSpace::Local, false);

    // Point 2 (Left)
    SplineComponent->AddSplinePoint(FVector(-R, 0.f, 0.f), ESplineCoordinateSpace::Local, false);
    SplineComponent->SetTangentsAtSplinePoint(2, FVector(0.f, -L, 0.f), FVector(0.f, -L, 0.f), ESplineCoordinateSpace::Local, false);

    // Point 3 (Back)
    SplineComponent->AddSplinePoint(FVector(0.f, -R, 0.f), ESplineCoordinateSpace::Local, false);
    SplineComponent->SetTangentsAtSplinePoint(3, FVector(L, 0.f, 0.f), FVector(L, 0.f, 0.f), ESplineCoordinateSpace::Local, true);

    SplineComponent->UpdateSpline();
}

void AAsteroidFieldGenerator::GenerateAsteroids()
{
    if (bIsGenerating) return;
    bIsGenerating = true;

    if (!SplineComponent)
    {
        bIsGenerating = false;
        return;
    }

    FRandomStream RandomStream(RandomSeed);

    // =========================================================
    // 1. CLEANUP PHASE
    // =========================================================
    
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

    for (TObjectPtr<AActor> Actor : SpawnedInteractables)
    {
        if (Actor && IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
    SpawnedInteractables.Empty();

    // =========================================================
    // 2. GENERATE VISUAL ASTEROIDS (HISM)
    // =========================================================
    
    TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> MeshToHISMMap;
    TArray<const FAsteroidTypeDefinition*> ValidVisualTypes;
    float TotalVisualWeight = 0.0f;

    for (const FAsteroidTypeDefinition& TypeDef : AsteroidTypes)
    {
        if (TypeDef.Mesh.IsNull() || TypeDef.Weight <= 0.0f) continue;
        TObjectPtr<UStaticMesh> LoadedMesh = TypeDef.Mesh.LoadSynchronous();
        if (!LoadedMesh) continue;

        if (!MeshToHISMMap.Contains(LoadedMesh))
        {
            FName HISMName = MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), FName(*FString::Printf(TEXT("VisualHISM_%s"), *LoadedMesh->GetName())));
            TObjectPtr<UHierarchicalInstancedStaticMeshComponent> NewHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, HISMName);
            NewHISM->SetupAttachment(SceneRoot);
            NewHISM->SetStaticMesh(LoadedMesh);
            NewHISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); 
            NewHISM->RegisterComponent();
            HISMComponents.Add(NewHISM);
            MeshToHISMMap.Add(LoadedMesh, NewHISM);
        }
        ValidVisualTypes.Add(&TypeDef);
        TotalVisualWeight += TypeDef.Weight;
    }

    if (TotalVisualWeight > 0.0f && NumberOfInstances > 0)
    {
        for (int32 i = 0; i < NumberOfInstances; ++i)
        {
            float RandomPick = RandomStream.FRandRange(0.f, TotalVisualWeight);
            const FAsteroidTypeDefinition* SelectedType = nullptr;
            float CurrentWeight = 0.f;
            for (const auto* Type : ValidVisualTypes) {
                if (RandomPick <= CurrentWeight + Type->Weight) { SelectedType = Type; break; }
                CurrentWeight += Type->Weight;
            }
            if(!SelectedType) SelectedType = ValidVisualTypes[0];

            FVector Pos;
            if (bFillArea) Pos = GetRandomPointInFieldVolume(RandomStream);
            else Pos = GetRandomPointInBeltVolume(RandomStream);

            FTransform Trans = CalculateInstanceTransform(Pos, RandomStream);
            TObjectPtr<UStaticMesh> MeshKey = SelectedType->Mesh.Get();

            if (MeshToHISMMap.Contains(MeshKey))
            {
                TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HISM = MeshToHISMMap[MeshKey];
                if (HISM)
                {
                    HISM->AddInstance(Trans);
                }
            }
        }
    }

    // =========================================================
    // 3. GENERATE INTERACTABLE ASTEROIDS (ACTORS)
    // =========================================================

    if (NumberOfInteractables > 0 && InteractableTypes.Num() > 0)
    {
        float TotalInteractableWeight = 0.0f;
        TArray<const FInteractableAsteroidDef*> ValidInteractableTypes;

        for (const FInteractableAsteroidDef& TypeDef : InteractableTypes)
        {
            if (TypeDef.ActorClass && TypeDef.Weight > 0.0f)
            {
                ValidInteractableTypes.Add(&TypeDef);
                TotalInteractableWeight += TypeDef.Weight;
            }
        }

        if (TotalInteractableWeight > 0.0f)
        {
            for (int32 i = 0; i < NumberOfInteractables; ++i)
            {
                float RandomPick = RandomStream.FRandRange(0.f, TotalInteractableWeight);
                const FInteractableAsteroidDef* SelectedType = nullptr;
                float CurrentWeight = 0.f;
                for (const auto* Type : ValidInteractableTypes) {
                    if (RandomPick <= CurrentWeight + Type->Weight) { SelectedType = Type; break; }
                    CurrentWeight += Type->Weight;
                }
                if (!SelectedType) SelectedType = ValidInteractableTypes[0];

                FVector InstancePos;
                if (bFillArea) InstancePos = GetRandomPointInFieldVolume(RandomStream);
                else InstancePos = GetRandomPointInBeltVolume(RandomStream);

                if (bRestrictInteractablesToPlane)
                {
                    InstancePos.Z = GameplayPlaneOffsetZ;
                }

                FRotator InstanceRot = FRotator::ZeroRotator;
                if (bRandomYaw) InstanceRot.Yaw = RandomStream.FRandRange(0.0f, 360.0f);
                
                if (bRandomPitchRoll && !bRestrictInteractablesToPlane) 
                {
                    InstanceRot.Pitch = RandomStream.FRandRange(0.0f, 360.0f);
                    InstanceRot.Roll = RandomStream.FRandRange(0.0f, 360.0f);
                }

                float Scale = RandomStream.FRandRange(MinScale, MaxScale);

                FVector WorldPos = GetTransform().TransformPosition(InstancePos);
                FRotator WorldRot = GetTransform().TransformRotation(InstanceRot.Quaternion()).Rotator();

                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = this;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
#if WITH_EDITOR
                SpawnParams.bTemporaryEditorActor = false; 
#endif

                AActor* NewActor = GetWorld()->SpawnActor<AActor>(SelectedType->ActorClass, WorldPos, WorldRot, SpawnParams);
                if (NewActor)
                {
                    NewActor->SetActorScale3D(FVector(Scale));
                    NewActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
                    SpawnedInteractables.Add(NewActor);
                }
            }
        }
    }

    bIsGenerating = false;
}

FVector AAsteroidFieldGenerator::GetRandomPointInBeltVolume(const FRandomStream& Stream) const
{
    if (!SplineComponent) return FVector::ZeroVector;

	const float SplineLength = SplineComponent->GetSplineLength();
    if (SplineLength < 1.0f) return SplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);

	const float DistanceAlongSpline = Stream.FRandRange(0.0f, SplineLength);
	const FVector PointOnSpline = SplineComponent->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);
	// Using Direction/Up allows the belt to twist if the spline twists, though usually it's flat
	const FVector UpVectorOnSpline = SplineComponent->GetUpVectorAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);
    // Standard Right Vector calculation
    const FVector DirectionOnSpline = SplineComponent->GetDirectionAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);
	const FVector RightVectorOnSpline = FVector::CrossProduct(DirectionOnSpline, UpVectorOnSpline).GetSafeNormal();

	const float OffsetWidth = Stream.FRandRange(-BeltWidth * 0.5f, BeltWidth * 0.5f);
	const float OffsetHeight = Stream.FRandRange(-BeltHeight * 0.5f, BeltHeight * 0.5f);

    // We assume Up is Z, Right is XY perp.
	FVector Position = PointOnSpline + (RightVectorOnSpline * OffsetWidth) + (UpVectorOnSpline * OffsetHeight);
	return Position; 
}

FVector AAsteroidFieldGenerator::GetRandomPointInFieldVolume(const FRandomStream& Stream) const
{
    if (!SplineComponent) return FVector::ZeroVector;

	FBoxSphereBounds SplineBoundsLocal = SplineComponent->GetLocalBounds();
	const float MaxRadiusXY = FMath::Max(SplineBoundsLocal.BoxExtent.X, SplineBoundsLocal.BoxExtent.Y);

	const float RandomAngle = Stream.FRandRange(0.0f, 2.0f * PI);
	const float RandomRadius = FMath::Sqrt(Stream.FRand()) * MaxRadiusXY; 
	
	const float OffsetX = FMath::Cos(RandomAngle) * RandomRadius;
	const float OffsetY = FMath::Sin(RandomAngle) * RandomRadius;
	const float OffsetZ = Stream.FRandRange(-FieldHeight * 0.5f, FieldHeight * 0.5f);

	FVector LocalPosition = SplineBoundsLocal.Origin + FVector(OffsetX, OffsetY, OffsetZ);
	return LocalPosition;
}

FTransform AAsteroidFieldGenerator::CalculateInstanceTransform(const FVector& LocalPosition, const FRandomStream& Stream) const
{
	const float Scale = Stream.FRandRange(MinScale, MaxScale);
	const FVector Scale3D(Scale); 

	FRotator Rotation = FRotator::ZeroRotator;
	if (bRandomYaw) Rotation.Yaw = Stream.FRandRange(0.0f, 360.0f);
	if (bRandomPitchRoll) 
	{
		Rotation.Pitch = Stream.FRandRange(0.0f, 360.0f);
		Rotation.Roll = Stream.FRandRange(0.0f, 360.0f);
	}

	return FTransform(Rotation, LocalPosition, Scale3D);
}