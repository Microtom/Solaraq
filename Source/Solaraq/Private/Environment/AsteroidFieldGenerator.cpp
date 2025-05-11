// Environment/AsteroidFieldGenerator.cpp

#include "Environment/AsteroidFieldGenerator.h"
#include "Components/SplineComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
// #include "Engine/StaticMesh.h" // Already in .h, but good to note where it would come from
#include "Math/RandomStream.h"       // For seeded random numbers
#include "Logging/SolaraqLogChannels.h" // Your custom logging, good!
#include "UObject/ConstructorHelpers.h" // For MakeUniqueObjectName

// Constructor: This is where we set up default values and create our components.
AAsteroidFieldGenerator::AAsteroidFieldGenerator()
{
    PrimaryActorTick.bCanEverTick = false; // Good for performance if we don't need to tick every frame.

    // Create the SceneRoot component and set it as the RootComponent for this Actor.
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    // Create the SplineComponent and attach it to the SceneRoot.
    SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
    SplineComponent->SetupAttachment(SceneRoot);
    SplineComponent->SetClosedLoop(true); // Let's make it a closed loop by default (e.g., for a ring).
    SplineComponent->ClearSplinePoints(false); // Clear any default points.

    // Let's define a default circular spline shape.
    // This provides a nice visual starting point in the editor.
    const float DefaultRadius = 10000.0f;
    const FVector P0 = FVector(DefaultRadius, 0.f, 0.f);
    const FVector P1 = FVector(0.f, DefaultRadius, 0.f);
    const FVector P2 = FVector(-DefaultRadius, 0.f, 0.f);
    const FVector P3 = FVector(0.f, -DefaultRadius, 0.f);

    SplineComponent->AddSplinePoint(P0, ESplineCoordinateSpace::Local, false);
    SplineComponent->AddSplinePoint(P1, ESplineCoordinateSpace::Local, false);
    SplineComponent->AddSplinePoint(P2, ESplineCoordinateSpace::Local, false);
    SplineComponent->AddSplinePoint(P3, ESplineCoordinateSpace::Local, false);

    // Setting tangents to make it circular. The factor 1.64f is an approximation for circularity with 4 points.
    const float TangentMagnitudeFactor = 1.64f ; // Adjusted slightly for better circle with 4 points
    
    const float TangentLength = DefaultRadius * TangentMagnitudeFactor; // Let's use what was there. Default tangents often work well too.
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
    SplineComponent->SetTangentAtSplinePoint(3, T3, ESplineCoordinateSpace::Local, false); // Corrected typo
    SplineComponent->UpdateSpline(); // IMPORTANT: Always call UpdateSpline after modifying points/tangents.

    // Default values for our editable properties.
    NumberOfInstances = 100;
    RandomSeed = 12345;
    bFillArea = false; // Default to a belt.
    BeltWidth = 2000.0f;
    BeltHeight = 500.0f;
    FieldHeight = 1000.0f; // Only used if bFillArea is true.
    MinScale = 0.5f;
    MaxScale = 1.5f;
    bRandomYaw = true;
    bRandomPitchRoll = true;
    bIsGenerating = false; // Initialize our safety flag.
}

void AAsteroidFieldGenerator::BeginPlay()
{
    Super::BeginPlay();
    // We typically generate asteroids in the editor via OnConstruction or the button.
    // You could uncomment the line below if you wanted to generate them at runtime when the game starts.
    // Make sure generation is fast enough if you do this!
    if (!GetWorld()->IsEditorWorld() && GetWorld()->IsGameWorld())
    {
        // GenerateAsteroids(); // Optionally generate at runtime
    }
}

// This is called when the Actor is placed in the editor or when its properties are changed
// (if "Run Construction Script on Drag" is enabled in Class Settings for this Actor).
void AAsteroidFieldGenerator::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // Regenerate asteroids whenever the actor is moved or settings are changed in the editor.
    // This gives instant feedback!
    GenerateAsteroids();
}

#if WITH_EDITOR
// This function is triggered after a property is changed in the Details panel of the editor.
void AAsteroidFieldGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // Get the name of the property that changed.
    const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

    // We only want to regenerate if relevant properties have changed.
    // This prevents unnecessary regeneration for properties that don't affect the visual outcome.
    if (PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, AsteroidTypes) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, NumberOfInstances) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, RandomSeed) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, bFillArea) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, BeltWidth) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, BeltHeight) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, FieldHeight) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, MinScale) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, MaxScale) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, bRandomYaw) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, bRandomPitchRoll) ||
        // Also, if the SplineComponent itself changes, we might want to regenerate.
        // However, spline changes often trigger OnConstruction anyway.
        // For direct spline point manipulation, OnConstruction usually handles it.
        (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AAsteroidFieldGenerator, SplineComponent))
       )
    {
        GenerateAsteroids();
    }
}
#endif // WITH_EDITOR

// The Big One! This function does all the work.
void AAsteroidFieldGenerator::GenerateAsteroids()
{
    // Safety check: if we're already generating, don't start another generation process.
    // This can prevent infinite loops or crashes if events trigger rapidly.
    if (bIsGenerating) return;
    bIsGenerating = true; // Set the flag

    // We absolutely need a SplineComponent to define the area.
    if (!SplineComponent)
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("AsteroidFieldGenerator %s: Missing Spline component! Cannot generate asteroids."), *GetName());
        bIsGenerating = false; // Reset flag before exiting
        return;
    }

    // --- 1. Cleanup Phase: Clear existing instances and HISM components ---
    // Before generating new asteroids, we need to remove any old ones.
    // This involves clearing instances from each HISM and then destroying the HISM component itself.
    UE_LOG(LogSolaraqSystem, Verbose, TEXT("AsteroidFieldGenerator %s: Clearing previous HISM components (%d found)."), *GetName(), HISMComponents.Num());
    for (TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HISM : HISMComponents)
    {
        if (HISM) // Always check if the pointer is valid
        {
            HISM->ClearInstances();        // Remove all instances from this HISM.
            HISM->UnregisterComponent();   // Unregister from the world.
            HISM->DestroyComponent();      // Mark for destruction.
        }
    }
    HISMComponents.Empty(); // Clear our array of HISM component pointers.

    // --- 2. Preparation Phase: Process AsteroidTypes and prepare for weighted selection ---
    // We need to:
    //   a) Load the meshes defined in AsteroidTypes.
    //   b) Create one HISM component for each *unique* static mesh.
    //   c) Collect data for weighted random selection of asteroid types.

    // This map will store a unique UStaticMesh* as a key and its corresponding HISMComponent as the value.
    // TObjectPtr ensures proper lifetime management with Unreal's UObject system.
    TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> MeshToHISMMap;

    // This array will store pointers to valid FAsteroidTypeDefinition structs that we can actually use.
    // We store pointers to avoid copying the structs and to easily access their 'Weight'.
    TArray<const FAsteroidTypeDefinition*> ValidSelectableTypes;
    float TotalWeight = 0.0f; // Sum of weights of all valid asteroid types.

    UE_LOG(LogSolaraqSystem, Verbose, TEXT("AsteroidFieldGenerator %s: Processing %d AsteroidTypes entries."), *GetName(), AsteroidTypes.Num());
    for (const FAsteroidTypeDefinition& TypeDef : AsteroidTypes)
    {
        // Validate the TypeDef:
        // - Mesh must be set (not null).
        // - Weight must be positive (otherwise, it would never be selected or cause issues).
        if (TypeDef.Mesh.IsNull())
        {
            UE_LOG(LogSolaraqSystem, Warning, TEXT("AsteroidFieldGenerator %s: AsteroidType entry has a null mesh. Skipping."), *GetName());
            continue;
        }
        if (TypeDef.Weight <= 0.0f)
        {
            UE_LOG(LogSolaraqSystem, Warning, TEXT("AsteroidFieldGenerator %s: AsteroidType with mesh %s has zero or negative weight (%.2f). Skipping."),
                *GetName(), *TypeDef.Mesh.ToString(), TypeDef.Weight);
            continue;
        }

        // Try to load the mesh. TSoftObjectPtr::LoadSynchronous() loads it immediately.
        TObjectPtr<UStaticMesh> LoadedMesh = TypeDef.Mesh.LoadSynchronous();
        if (!LoadedMesh)
        {
            UE_LOG(LogSolaraqSystem, Warning, TEXT("AsteroidFieldGenerator %s: Failed to load mesh %s. Skipping."), *GetName(), *TypeDef.Mesh.ToString());
            continue;
        }

        // Now, check if we already have a HISM for this specific mesh.
        if (!MeshToHISMMap.Contains(LoadedMesh))
        {
            // If not, create a new HISM component for this mesh.
            // We need a unique name for each new component. MakeUniqueObjectName helps with this.
            FName HISMName = MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), FName(*FString::Printf(TEXT("AsteroidHISM_%s"), *LoadedMesh->GetName())));
            
            // NewObject is how you create UObjects dynamically in C++.
            TObjectPtr<UHierarchicalInstancedStaticMeshComponent> NewHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, HISMName);
            if (NewHISM)
            {
                NewHISM->SetupAttachment(SceneRoot);       // Attach to our actor's root.
                NewHISM->SetStaticMesh(LoadedMesh);        // Assign the loaded mesh to this HISM.
                NewHISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); // Or your desired collision
                NewHISM->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName); // Standard profile
                NewHISM->RegisterComponent();              // IMPORTANT: Make the component active in the world.
                
                HISMComponents.Add(NewHISM);              // Add to our main list for tracking and future cleanup.
                MeshToHISMMap.Add(LoadedMesh, NewHISM);   // Add to our map for quick lookup.
                UE_LOG(LogSolaraqSystem, Verbose, TEXT("AsteroidFieldGenerator %s: Created HISM '%s' for mesh %s."), *GetName(), *HISMName.ToString(), *LoadedMesh->GetName());
            }
            else
            {
                UE_LOG(LogSolaraqSystem, Error, TEXT("AsteroidFieldGenerator %s: Failed to create NewHISM for mesh %s. Skipping this type."), *GetName(), *LoadedMesh->GetName());
                continue; // Skip this TypeDef if HISM creation failed.
            }
        }
        
        // If we've reached here, the mesh is loaded, and a HISM exists for it.
        // Add this type to our list of types we can pick from, and add its weight to the total.
        ValidSelectableTypes.Add(&TypeDef); // Store a pointer to the original TypeDef.
        TotalWeight += TypeDef.Weight;
    }

    // If there are no valid types to select from (e.g., all meshes failed to load, or all weights were zero),
    // then there's nothing to generate.
    if (ValidSelectableTypes.IsEmpty() || TotalWeight <= 0.0f)
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("AsteroidFieldGenerator %s: No valid asteroid types to generate from (check meshes and weights). TotalWeight: %.2f. Aborting generation."), *GetName(), TotalWeight);
        bIsGenerating = false; // Reset flag
        return;
    }
    UE_LOG(LogSolaraqSystem, Log, TEXT("AsteroidFieldGenerator %s: Prepared %d unique HISM components for %d valid selectable asteroid types. Total weight: %.2f"),
        *GetName(), MeshToHISMMap.Num(), ValidSelectableTypes.Num(), TotalWeight);


    // --- 3. Instantiation Phase: Create and place asteroid instances ---
    if (NumberOfInstances <= 0)
    {
        UE_LOG(LogSolaraqSystem, Log, TEXT("AsteroidFieldGenerator %s: NumberOfInstances is %d. No instances will be generated."), *GetName(), NumberOfInstances);
        bIsGenerating = false; // Reset flag
        return;
    }

    // Initialize our random number stream with the specified seed.
    // This ensures that if the seed is the same, the "random" sequence will also be the same,
    // leading to a repeatable asteroid field layout.
    FRandomStream RandomStream(RandomSeed);
    int32 TotalInstancesAdded = 0;

    // Loop to create the desired number of asteroid instances.
    for (int32 i = 0; i < NumberOfInstances; ++i)
    {
        // --- Weighted Random Selection of Asteroid Type ---
        // Pick a random value between 0 and TotalWeight.
        float RandomPick = RandomStream.FRandRange(0.f, TotalWeight);
        const FAsteroidTypeDefinition* SelectedType = nullptr;
        float CurrentCumulativeWeight = 0.f;

        // Iterate through our valid types. Imagine all types lined up, each occupying a segment
        // proportional to its weight. We "walk" along this line until our RandomPick falls into a segment.
        for (const FAsteroidTypeDefinition* TypePtr : ValidSelectableTypes)
        {
            // TypePtr should always be valid as we only added valid pointers.
            if (RandomPick <= CurrentCumulativeWeight + TypePtr->Weight)
            {
                SelectedType = TypePtr;
                break; // Found our type!
            }
            CurrentCumulativeWeight += TypePtr->Weight;
        }

        // Fallback: If something went wrong (e.g., floating point precision with TotalWeight),
        // or if RandomPick was exactly TotalWeight and the last item wasn't picked,
        // just pick the first valid type. This should be rare.
        if (!SelectedType)
        {
            if (!ValidSelectableTypes.IsEmpty())
            {
                SelectedType = ValidSelectableTypes[0];
                UE_LOG(LogSolaraqSystem, Warning, TEXT("AsteroidFieldGenerator %s: Weighted selection fallback triggered. Using first valid type."), *GetName());
            }
            else
            {
                UE_LOG(LogSolaraqSystem, Error, TEXT("AsteroidFieldGenerator %s: Weighted selection failed and no valid types available. This shouldn't happen."), *GetName());
                continue; // Should not be reachable if initial checks passed.
            }
        }
        
        // Now we have a SelectedType. Get its mesh.
        // Since TSoftObjectPtr::Get() returns nullptr if not loaded, and we loaded them earlier,
        // this should be safe. But a paranoid check or re-load doesn't hurt.
        TObjectPtr<UStaticMesh> MeshForInstance = SelectedType->Mesh.Get();
        if (!MeshForInstance)
        {
            // Mesh might have been garbage collected if not referenced strongly elsewhere,
            // or if it was never successfully loaded into the MeshToHISMMap.
            // Attempt to re-load it.
            MeshForInstance = SelectedType->Mesh.LoadSynchronous();
            if (!MeshForInstance)
            {
                UE_LOG(LogSolaraqSystem, Error, TEXT("AsteroidFieldGenerator %s: Failed to get/load mesh %s for selected type. Skipping instance."), *GetName(), *SelectedType->Mesh.ToString());
                continue;
            }
        }

        // Find the HISM component associated with this mesh.
        const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* FoundHISM_PtrPtr = MeshToHISMMap.Find(MeshForInstance);
        if (FoundHISM_PtrPtr && *FoundHISM_PtrPtr) // Check if pointer-to-pointer is valid, then check if the TObjectPtr itself is valid
        {
            TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TargetHISM = *FoundHISM_PtrPtr;

            // Determine the base position for this asteroid.
            FVector InstanceBasePosition;
            if (bFillArea)
            {
                InstanceBasePosition = GetRandomPointInFieldVolume(RandomStream);
            }
            else
            {
                InstanceBasePosition = GetRandomPointInBeltVolume(RandomStream);
            }

            // Calculate the final transform (position, rotation, scale).
            FTransform InstanceTransform = CalculateInstanceTransform(InstanceBasePosition, RandomStream);
            
            // Add the instance to the HISM! This is the actual spawning.
            TargetHISM->AddInstance(InstanceTransform);
            TotalInstancesAdded++;
        }
        else
        {
             UE_LOG(LogSolaraqSystem, Error, TEXT("AsteroidFieldGenerator %s: Could not find HISM for selected mesh %s! This indicates an internal logic error."), *GetName(), *MeshForInstance->GetName());
        }
    }

    UE_LOG(LogSolaraqSystem, Log, TEXT("AsteroidFieldGenerator %s: Successfully generated %d total instances across %d HISM components using weighted selection."), *GetName(), TotalInstancesAdded, MeshToHISMMap.Num());
    bIsGenerating = false; // Reset the flag, generation is complete.
}

// --- Helper Functions ---

// Gets a random point within a belt-like volume defined by the spline.
FVector AAsteroidFieldGenerator::GetRandomPointInBeltVolume(const FRandomStream& Stream) const
{
    // Ensure SplineComponent is valid (should be, as GenerateAsteroids checks, but defensive coding is good)
	if (!SplineComponent) return FVector::ZeroVector;

	const float SplineLength = SplineComponent->GetSplineLength();
	if (SplineLength < KINDA_SMALL_NUMBER) // Avoid division by zero or issues with tiny splines
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("AsteroidFieldGenerator %s: Spline length is very small in GetRandomPointInBeltVolume."), *GetName());
        return SplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local); // Return start point
    }

    // Pick a random distance along the spline.
	const float DistanceAlongSpline = Stream.FRandRange(0.0f, SplineLength);
    // Get the location, direction (tangent), and up vector at that point on the spline.
    // These are in Local space relative to the SplineComponent.
	const FVector PointOnSpline = SplineComponent->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);
	const FVector DirectionOnSpline = SplineComponent->GetDirectionAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);
	const FVector UpVectorOnSpline = SplineComponent->GetUpVectorAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);

    // Calculate the "right" vector relative to the spline's orientation.
	const FVector RightVectorOnSpline = FVector::CrossProduct(DirectionOnSpline, UpVectorOnSpline).GetSafeNormal();

    // Random offsets for width (along RightVector) and height (along UpVector).
	const float OffsetWidth = Stream.FRandRange(-BeltWidth * 0.5f, BeltWidth * 0.5f);
	const float OffsetHeight = Stream.FRandRange(-BeltHeight * 0.5f, BeltHeight * 0.5f);

    // Combine the point on the spline with the offsets to get the final position.
	FVector Position = PointOnSpline + (RightVectorOnSpline * OffsetWidth) + (UpVectorOnSpline * OffsetHeight);

	return Position; 
}

// Gets a random point within a volume roughly defined by the spline's extents.
FVector AAsteroidFieldGenerator::GetRandomPointInFieldVolume(const FRandomStream& Stream) const
{
    // Ensure SplineComponent is valid
    if (!SplineComponent) return FVector::ZeroVector;

	FBoxSphereBounds SplineBoundsLocal = SplineComponent->GetLocalBounds();
    // Using local bounds is simpler and more aligned with how instances are placed (in local space).
    // The original code calculated world bounds then converted back, which is fine, but GetLocalBounds() is more direct.

	// We'll generate points within a cylinder or flattened sphere defined by these bounds.
    // The original code used a disk shape projection and then offset Z. This is a good approach.
	const float MaxRadiusXY = FMath::Max(SplineBoundsLocal.BoxExtent.X, SplineBoundsLocal.BoxExtent.Y);

    // Generate a random point in a disk (polar coordinates).
    // Using Sqrt(Stream.FRand()) gives a more uniform distribution across the disk's area.
	const float RandomAngle = Stream.FRandRange(0.0f, 2.0f * PI);
	const float RandomRadius = FMath::Sqrt(Stream.FRand()) * MaxRadiusXY; 
	
    // Convert polar to Cartesian coordinates relative to the spline's local center.
	const float OffsetX = FMath::Cos(RandomAngle) * RandomRadius;
	const float OffsetY = FMath::Sin(RandomAngle) * RandomRadius;
    // Random Z offset within the defined FieldHeight.
	const float OffsetZ = Stream.FRandRange(-FieldHeight * 0.5f, FieldHeight * 0.5f);

    // The final position is the center of the spline's local bounds plus our random offsets.
    // SplineBoundsLocal.Origin is the center of the local bounding box.
	FVector LocalPosition = SplineBoundsLocal.Origin + FVector(OffsetX, OffsetY, OffsetZ);

    // Note: The original code calculated spline bounds in World space, then transformed the random point
    // back to Local space. Generating directly in Local space using LocalBounds is often simpler
    // if the final instance transforms are also in Local space relative to the Actor's root.
    // Since AddInstance takes local transforms, this is consistent.
	return LocalPosition;
}

// Calculates the scale and rotation for an individual asteroid instance.
FTransform AAsteroidFieldGenerator::CalculateInstanceTransform(const FVector& LocalPosition, const FRandomStream& Stream) const
{
    // Random scale within the defined min/max range.
	const float Scale = Stream.FRandRange(MinScale, MaxScale);
	const FVector Scale3D(Scale); // Uniform scaling.

    // Random rotation.
	FRotator Rotation = FRotator::ZeroRotator;
	if (bRandomYaw)
	{
		Rotation.Yaw = Stream.FRandRange(0.0f, 360.0f);
	}
	if (bRandomPitchRoll) // If true, randomize both pitch and roll.
	{
		Rotation.Pitch = Stream.FRandRange(0.0f, 360.0f);
		Rotation.Roll = Stream.FRandRange(0.0f, 360.0f);
	}

    // Construct the final transform using the provided LocalPosition, calculated Rotation, and Scale.
	return FTransform(Rotation, LocalPosition, Scale3D);
}