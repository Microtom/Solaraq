// Fill out your copyright notice in the Description page of Project Settings.

#include "Environment/AsteroidFieldGenerator.h" // Adjust path
#include "Components/SplineComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/KismetMathLibrary.h" // For RandomPointInBoundingBox, etc.
#include "Math/RandomStream.h"
#include "Logging/SolaraqLogChannels.h" // Optional: Use your custom logging

AAsteroidFieldGenerator::AAsteroidFieldGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	SplineComponent->SetupAttachment(SceneRoot);
	SplineComponent->SetClosedLoop(true);
	SplineComponent->ClearSplinePoints(false); // Remove default points

	const float DefaultRadius = 10000.0f;
	const FVector P0 = FVector(DefaultRadius, 0.f, 0.f);
	const FVector P1 = FVector(0.f, DefaultRadius, 0.f);
	const FVector P2 = FVector(-DefaultRadius, 0.f, 0.f);
	const FVector P3 = FVector(0.f, -DefaultRadius, 0.f);

	// Add the four cardinal points
	SplineComponent->AddSplinePoint(P0, ESplineCoordinateSpace::Local, false); // Index 0
	SplineComponent->AddSplinePoint(P1, ESplineCoordinateSpace::Local, false); // Index 1
	SplineComponent->AddSplinePoint(P2, ESplineCoordinateSpace::Local, false); // Index 2
	SplineComponent->AddSplinePoint(P3, ESplineCoordinateSpace::Local, false); // Index 3

	// --- Set Tangents for a Circular Shape ---
	// The tangent magnitude affects the "roundness".
	// A common value related to Bezier curves forming circles is roughly (4/3)*tan(pi/8) * R,
	// or often approximated. Let's use a factor of the radius.
	// Try a value like 1.0 * Radius or slightly more. Experiment with this factor!
	const float TangentMagnitudeFactor = 1.64f; // <<< TUNE THIS VALUE (Try values between 0.8 and 1.5 maybe)
	const float TangentLength = DefaultRadius * TangentMagnitudeFactor;

	// Calculate tangent vectors (Perpendicular to the radius vector for a circle)
	const FVector T0 = FVector(0.f, TangentLength, 0.f);  // Tangent at P0 (+Y direction)
	const FVector T1 = FVector(-TangentLength, 0.f, 0.f); // Tangent at P1 (-X direction)
	const FVector T2 = FVector(0.f, -TangentLength, 0.f); // Tangent at P2 (-Y direction)
	const FVector T3 = FVector(TangentLength, 0.f, 0.f);  // Tangent at P3 (+X direction)

	// Set tangents and point types
	// For a smooth curve, the ArriveTangent at point N should ideally be -LeaveTangent at point N.
	// SetTangentAtSplinePoint sets BOTH arrive and leave tangent appropriately for Curve type.
	SplineComponent->SetSplinePointType(0, ESplinePointType::Curve, false);
	SplineComponent->SetTangentAtSplinePoint(0, T0, ESplineCoordinateSpace::Local, false);

	SplineComponent->SetSplinePointType(1, ESplinePointType::Curve, false);
	SplineComponent->SetTangentAtSplinePoint(1, T1, ESplineCoordinateSpace::Local, false);

	SplineComponent->SetSplinePointType(2, ESplinePointType::Curve, false);
	SplineComponent->SetTangentAtSplinePoint(2, T2, ESplineCoordinateSpace::Local, false);

	SplineComponent->SetSplinePointType(3, ESplinePointType::Curve, false);
	SplineComponent->SetTangentAtSplinePoint(3, T3, ESplineCoordinateSpace::Local, false);

	// --- IMPORTANT: Update the spline AFTER setting points and tangents ---
	SplineComponent->UpdateSpline(); // Apply changes

	AsteroidInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("AsteroidInstances"));
	AsteroidInstances->SetupAttachment(SceneRoot);
	AsteroidInstances->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	AsteroidInstances->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
}

void AAsteroidFieldGenerator::BeginPlay()
{
	Super::BeginPlay();
	// Generate asteroids at runtime if needed (e.g., if parameters could change)
	// Might want a bool UPROPERTY like bGenerateOnBeginPlay
	if (!GetWorld()->IsEditorWorld()) // Avoid double generation if OnConstruction ran
	{
		// GenerateAsteroids(); // Uncomment if runtime generation is desired
	}
}

void AAsteroidFieldGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Generate in editor for preview
	GenerateAsteroids();
}

#if WITH_EDITOR
void AAsteroidFieldGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Regenerate if relevant properties change in the editor
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// Add checks for your property names here
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, AsteroidMesh) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, NumberOfInstances) ||
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
	// Note: Changes to the spline itself are handled by OnConstruction when the actor is moved/updated.
}
#endif

void AAsteroidFieldGenerator::GenerateAsteroids()
{
	// Prevent recursive calls from PostEditChangeProperty if generation modifies something
	if (bIsGenerating) return;
	bIsGenerating = true;

	// Ensure components are valid
	if (!SplineComponent || !AsteroidInstances)
	{
		UE_LOG(LogSolaraqSystem, Error, TEXT("AsteroidFieldGenerator %s: Missing Spline or HISM component!"), *GetName());
		bIsGenerating = false;
		return;
	}

	// Set the mesh on the HISM component
	UStaticMesh* Mesh = AsteroidMesh.LoadSynchronous(); // Load mesh if not already loaded
	if (!Mesh)
	{
		UE_LOG(LogSolaraqSystem, Warning, TEXT("AsteroidFieldGenerator %s: No AsteroidMesh assigned."), *GetName());
		AsteroidInstances->ClearInstances(); // Clear any old instances if mesh removed
		AsteroidInstances->SetStaticMesh(nullptr);
		bIsGenerating = false;
		return;
	}
	AsteroidInstances->SetStaticMesh(Mesh);

	// Clear previous instances
	AsteroidInstances->ClearInstances();

	if (NumberOfInstances <= 0)
	{
		bIsGenerating = false;
		return; // Nothing to generate
	}

	FRandomStream RandomStream(RandomSeed); // Use the seed

	for (int32 i = 0; i < NumberOfInstances; ++i)
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

		// Calculate final transform including rotation and scale
		FTransform InstanceTransform = CalculateInstanceTransform(InstanceBasePosition, RandomStream);

		// Add the instance to the HISM component
		AsteroidInstances->AddInstance(InstanceTransform);
	}

	UE_LOG(LogSolaraqSystem, Log, TEXT("AsteroidFieldGenerator %s: Generated %d instances."), *GetName(), AsteroidInstances->GetInstanceCount());
	bIsGenerating = false;
}

FVector AAsteroidFieldGenerator::GetRandomPointInBeltVolume(const FRandomStream& Stream) const
{
	const float SplineLength = SplineComponent->GetSplineLength();
	// Use Actor's location as fallback ONLY if spline is truly invalid
	if (SplineLength < KINDA_SMALL_NUMBER) return FVector::ZeroVector; // Return local origin (0,0,0)

	// 1. Get random point ON the spline in LOCAL space
	const float DistanceAlongSpline = Stream.FRandRange(0.0f, SplineLength);
	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvv CHANGE HERE vvvvvvvvvvvvvvvvvvvvvvvvvvvv
	const FVector PointOnSpline = SplineComponent->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);
	const FVector DirectionOnSpline = SplineComponent->GetDirectionAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);
	const FVector UpVectorOnSpline = SplineComponent->GetUpVectorAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ CHANGE HERE ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	// 2. Calculate perpendicular direction (Right vector) - calculations remain the same conceptually
	const FVector RightVectorOnSpline = FVector::CrossProduct(DirectionOnSpline, UpVectorOnSpline).GetSafeNormal();

	// 3. Get random offsets
	const float OffsetWidth = Stream.FRandRange(-BeltWidth * 0.5f, BeltWidth * 0.5f);
	const float OffsetHeight = Stream.FRandRange(-BeltHeight * 0.5f, BeltHeight * 0.5f);

	// 4. Calculate final position (already relative to Actor origin)
	FVector Position = PointOnSpline + (RightVectorOnSpline * OffsetWidth) + (UpVectorOnSpline * OffsetHeight);

	return Position; // This is now a LOCAL position
}

FVector AAsteroidFieldGenerator::GetRandomPointInFieldVolume(const FRandomStream& Stream) const
{
	// --- Simple implementation assuming a mostly flat, circular spline ---
	FBox SplineBounds(ForceInit);
	const int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
	if (NumPoints < 2) return FVector::ZeroVector; // Return Local origin

	// Calculate bounds in WORLD space first (easier for complex splines)
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

	// Calculate the position in WORLD space
	FVector WorldPosition = FVector(Center.X + OffsetX, Center.Y + OffsetY, Center.Z + OffsetZ);

	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvv NEW STEP vvvvvvvvvvvvvvvvvvvvvvvvvvvv
	// Convert the calculated world position into the Actor's LOCAL space
	FVector LocalPosition = GetTransform().InverseTransformPosition(WorldPosition);
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ NEW STEP ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	// OPTIONAL Check: Point-in-polygon test could be done here using LOCAL coordinates

	return LocalPosition; // Return the LOCAL position
}

FTransform AAsteroidFieldGenerator::CalculateInstanceTransform(const FVector& LocalPosition, const FRandomStream& Stream) const
{
	// 1. Scale (remains the same)
	const float Scale = Stream.FRandRange(MinScale, MaxScale);
	const FVector Scale3D(Scale);

	// 2. Rotation (remains the same)
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

	// 3. Combine into Transform using the provided LOCAL Position
	//    The FTransform constructor correctly interprets this Location
	//    relative to the component it's added to (or its parent).
	return FTransform(Rotation, LocalPosition, Scale3D);
}
