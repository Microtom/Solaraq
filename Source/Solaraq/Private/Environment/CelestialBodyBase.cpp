// Fill out your copyright notice in the Description page of Project Settings.

#include "Environment/CelestialBodyBase.h" // Adjust path as needed
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Pawns/SolaraqShipBase.h"        // Adjust path as needed
#include "Components/PrimitiveComponent.h" // For AddForce
#include "Components/BoxComponent.h" // For accessing ship's physics root
#include "Logging/SolaraqLogChannels.h" // Optional: Use your custom logging


ACelestialBodyBase::ACelestialBodyBase()
{
    PrimaryActorTick.bCanEverTick = true; // Need tick to apply gravity/scaling continuously
    PrimaryActorTick.bStartWithTickEnabled = false; // Start disabled, enable only when ships are near

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    BodyMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMeshComponent->SetupAttachment(SceneRoot);
    BodyMeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName); // Or a custom profile

    InfluenceSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("InfluenceSphere"));
    InfluenceSphereComponent->SetupAttachment(SceneRoot);
    InfluenceSphereComponent->SetCollisionProfileName(FName("OverlapAllDynamic")); // Only overlap Pawns potentially
    InfluenceSphereComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InfluenceSphereComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	InfluenceSphereComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InfluenceSphereComponent->bHiddenInGame = false; // Set to true later for release

    // Bind overlap events
    InfluenceSphereComponent->OnComponentBeginOverlap.AddDynamic(this, &ACelestialBodyBase::OnInfluenceOverlapBegin);
    InfluenceSphereComponent->OnComponentEndOverlap.AddDynamic(this, &ACelestialBodyBase::OnInfluenceOverlapEnd);

    // --- Scaling Start Sphere --- <<< NEW
    ScalingSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("ScalingSphere"));
    ScalingSphereComponent->SetupAttachment(SceneRoot);
    ScalingSphereComponent->SetCollisionProfileName(FName("NoCollision")); // This one is purely visual/range check
    ScalingSphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Does NOT trigger overlaps
    ScalingSphereComponent->bHiddenInGame = false; // Keep visible for debugging scale range
    ScalingSphereComponent->ShapeColor = FColor::Green; // Optional: Distinguish visually
    ScalingSphereComponent->ComponentTags.Add(FName("ScalingBoundary")); // Tag for potential editor visualization scripts
    
    
    // Replication - Basic setup, assuming mostly static bodies
    bReplicates = true;
    SetReplicatingMovement(false); // If static
    bNetLoadOnClient = true;

    // Update radii based on default property values AFTER creation
    UpdateInfluenceSphereRadius();
    UpdateScalingSphereRadius();
    ValidateRadii();
}

// --- PostEditChangeProperty ---
#if WITH_EDITOR
// This specifically handles the case where a property is changed in the Details panel.
void ACelestialBodyBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
    bool bRadiiChanged = false;

    // Check if relevant properties changed
    if (PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialBodyBase, InfluenceRadius))
    {
        ValidateRadii(); // Validate before updating component
        UpdateInfluenceSphereRadius();
        bRadiiChanged = true;
    }
    else if (PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialBodyBase, ScalingRadius))
    {
        ValidateRadii(); // Validate before updating component
        UpdateScalingSphereRadius();
        bRadiiChanged = true;
    }
    else if (PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialBodyBase, MinScaleDistance))
    {
        bRadiiChanged = true; // Need to re-validate relative to ScalingRadius
    }


    // Re-validate relative distances if any relevant property changed
    if (bRadiiChanged)
    {
        ValidateRadii(); // Ensure ScalingRadius <= InfluenceRadius after change
        // Update components again if ValidateRadii clamped values
        UpdateInfluenceSphereRadius();
        UpdateScalingSphereRadius();

        // Check MinScaleDistance relative to the potentially updated ScalingRadius
        if (MaxScalingDistance > 0 && MinScaleDistance >= MaxScalingDistance)
        {
            //UE_LOG(LogSolaraqCelestials, Warning, TEXT("CelestialBody '%s' PostEditChange: MinScaleDistance (%.1f) >= ScalingRadius (%.1f). Clamping MinScaleDistance."),
             //  *GetName(), MinScaleDistance, MaxScalingDistance);
            MinScaleDistance = MaxScalingDistance * 0.95f; // Example: Clamp it slightly inside
        }
        if (MinScaleDistance < BodyMeshComponent->Bounds.SphereRadius)
        {
           // UE_LOG(LogSolaraqCelestials, Warning, TEXT("CelestialBody '%s' PostEditChange: MinScaleDistance (%.1f) < BodyMesh radius (%.1f). Ship might clip."),
            //   *GetName(), MinScaleDistance, BodyMeshComponent->Bounds.SphereRadius);
        }
    }
}
#endif // WITH_EDITOR

void ACelestialBodyBase::BeginPlay()
{
    Super::BeginPlay();

    // Ensure the radius is correct based on potentially edited values when the game starts.
    // OnConstruction usually handles this, but BeginPlay is a safe place too.
    ValidateRadii();
    UpdateInfluenceSphereRadius();
    UpdateScalingSphereRadius();

    // --- Validation checks in BeginPlay remain important ---
    if (MaxInfluenceDistance <= 0) // Check if radius is valid
    {
        UE_LOG(LogSolaraqCelestials, Error, TEXT("CelestialBody '%s': MaxInfluenceDistance (based on InfluenceRadius) is zero or negative! Effects will not work."), *GetName());
        return; // Early out if radius is invalid
    }

    if (MinScaleDistance > MaxInfluenceDistance)
    {
        //UE_LOG(LogSolaraqCelestials, Warning, TEXT("CelestialBody '%s': MinScaleDistance (%.1f) is >= MaxInfluenceDistance (%.1f) at BeginPlay. Scaling will not function correctly."),
         //   *GetName(), MinScaleDistance, MaxInfluenceDistance);
        // Consider clamping MinScaleDistance here as well if needed for runtime safety
        // MinScaleDistance = MaxInfluenceDistance * 0.9f;
    }

    if (MinScaleDistance >= MaxScalingDistance && MaxScalingDistance > 0)
    {
      //  UE_LOG(LogSolaraqCelestials, Warning, TEXT("CelestialBody '%s': MinScaleDistance (%.1f) >= MaxScalingDistance (%.1f) at BeginPlay. Scaling may not function correctly."),
      //     *GetName(), MinScaleDistance, MaxScalingDistance);
    }
    
    if (MinScaleDistance < BodyMeshComponent->Bounds.SphereRadius)
    {
       // UE_LOG(LogSolaraqCelestials, Warning, TEXT("CelestialBody '%s': MinScaleDistance (%.1f) < BodyMesh radius (%.1f) at BeginPlay. Ship might clip."),
       //    *GetName(), MinScaleDistance, BodyMeshComponent->Bounds.SphereRadius);
    }
    // ------------------------------------------------------
}

void ACelestialBodyBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (AffectedShips.Num() > 0 && HasAuthority()) // Server applies physics/scaling changes
    {
        for (int32 i = AffectedShips.Num() - 1; i >= 0; --i)
        {
            ASolaraqShipBase* Ship = AffectedShips[i];
            if (IsValid(Ship))
            {
                ApplyEffectsToShip(Ship, DeltaTime); // ApplyEffectsToShip handles logic based on distance now
            }
            else
            {
                AffectedShips.RemoveAt(i);
            }
        }
    }

    if (AffectedShips.Num() == 0 && PrimaryActorTick.bCanEverTick)
    {
        SetActorTickEnabled(false);
    }
}

void ACelestialBodyBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Update spheres whenever the actor is constructed or modified in the editor
    ValidateRadii();
    UpdateInfluenceSphereRadius();
    UpdateScalingSphereRadius();
    
    // Also re-validate distances here for editor feedback
    if (MaxScalingDistance > 0 && MinScaleDistance >= MaxScalingDistance)
    {
      //  UE_LOG(LogSolaraqCelestials, Warning, TEXT("CelestialBody '%s' OnConstruction: MinScaleDistance (%.1f) >= ScalingRadius (%.1f). Adjust values."),
      //     *GetName(), MinScaleDistance, MaxScalingDistance);
    }
    if (MinScaleDistance < BodyMeshComponent->Bounds.SphereRadius)
    {
     //   UE_LOG(LogSolaraqCelestials, Warning, TEXT("CelestialBody '%s' OnConstruction: MinScaleDistance (%.1f) < BodyMesh radius (%.1f). Ship might clip."),
     //      *GetName(), MinScaleDistance, BodyMeshComponent->Bounds.SphereRadius);
    }
}

void ACelestialBodyBase::OnInfluenceOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    ASolaraqShipBase* Ship = Cast<ASolaraqShipBase>(OtherActor);
	if (Ship && OtherComp == Ship->GetCollisionAndPhysicsRoot())
	{
		if (!AffectedShips.Contains(Ship))
		{
			AffectedShips.AddUnique(Ship);
			UE_LOG(LogSolaraqSystem, Log, TEXT("CelestialBody '%s': Ship '%s' entered GRAVITY influence."), *GetName(), *Ship->GetName());
			if (!IsActorTickEnabled())
			{
				Ship->SetUnderScalingEffect_Server(true);
				SetActorTickEnabled(true);
			}
		}
	}
}

void ACelestialBodyBase::OnInfluenceOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ASolaraqShipBase* Ship = Cast<ASolaraqShipBase>(OtherActor);
	if (Ship && OtherComp == Ship->GetCollisionAndPhysicsRoot())
	{
		int32 RemovedCount = AffectedShips.Remove(Ship);
		if (RemovedCount > 0)
		{
			UE_LOG(LogSolaraqSystem, Log, TEXT("CelestialBody '%s': Ship '%s' left GRAVITY influence."), *GetName(), *Ship->GetName());
			if (IsValid(Ship))
			{
				Ship->Client_ResetVisualScale(); // Ensure scale is reset on client
				if (HasAuthority()) // Should always be true if called from server's overlap event
				{
					Ship->SetUnderScalingEffect_Server(false); // For weapon disabling
					Ship->SetEffectiveScaleFactor_Server(1.0f); // Reset effective scale for physics
				}
			}
		}
	}
}

void ACelestialBodyBase::ApplyEffectsToShip(ASolaraqShipBase* Ship, float DeltaTime)
{
	if (!Ship || !Ship->GetCollisionAndPhysicsRoot() || !Ship->GetCollisionAndPhysicsRoot()->IsSimulatingPhysics()) return;

	if (!HasAuthority()) return; // Ensure server-side

	const FVector BodyLocation = GetActorLocation();
	const FVector ShipLocation = Ship->GetActorLocation();
	const float Distance = FVector::Dist(BodyLocation, ShipLocation);

	// --- 1. Apply Gravity ---
	const FVector GravityForce = CalculateGravityForce(Distance, ShipLocation);
	Ship->GetCollisionAndPhysicsRoot()->AddForce(GravityForce, NAME_None, false);

	// --- 2. Calculate and Apply Visual Scaling ---
	const float TargetScaleFactor = CalculateShipScaleFactor(Distance); // This is already clamped (0.01 to 1.0)
	Ship->Client_SetVisualScale(TargetScaleFactor); 

	// --- 3. Update Server-Side Scaling State on the Ship (for weapon disabling) ---
	Ship->SetUnderScalingEffect_Server(!FMath::IsNearlyEqual(TargetScaleFactor, 1.0f));

	// --- 4. Update Server-Side Effective Scale for Physics Adjustments ---
	Ship->SetEffectiveScaleFactor_Server(TargetScaleFactor); 

    // Optional Debug Logging for Gravity
    // UE_LOG(LogSolaraqSystem, VeryVerbose, TEXT("Applying Gravity: %s at Dist: %.1f (Influence Radius: %.1f)"), *GravityForce.ToString(), Distance, MaxInfluenceDistance);
}

float ACelestialBodyBase::CalculateShipScaleFactor(float Distance) const
{
    // Optional: Log the input values for debugging if needed
	 //UE_LOG(LogSolaraqCelestials, Warning, TEXT("CalcScale Start: InputDist=%.2f, MaxScaleDist=%.2f, MinScaleDist=%.2f"),
	 //		Distance, MaxScalingDistance, MinScaleDistance);

	// --- Check 1: Ship is outside or exactly at the edge of the scaling sphere ---
	// If the distance is greater than or equal to the maximum distance where scaling occurs,
	// the scale factor should be 1.0 (no scaling).
	if (Distance >= MaxScalingDistance)
	{
		// UE_LOG(LogSolaraqSystem, Verbose, TEXT("CalcScale Result: >= Max (1.0)"));
		return 1.0f;
	}

	// --- Check 2: Ship is at or closer than the minimum distance ---
	// If the distance is less than or equal to the minimum distance where full scaling is applied,
	// return the defined minimum scale factor.
	if (Distance <= MinScaleDistance)
	{
		// UE_LOG(LogSolaraqSystem, Verbose, TEXT("CalcScale Result: <= Min (%.2f)"), MinShipScaleFactor);
		return MinShipScaleFactor;
	}

	// --- Check 3: Ship is within the interpolation zone (between MinScaleDistance and MaxScalingDistance) ---
	// Calculate the fraction of the way the ship is through the scaling zone.
	// GetRangePct finds how far 'Distance' is between 'Min' and 'Max', returning a 0-1 value.
	// CORRECT ORDER: FMath::GetRangePct(Value, MinValue, MaxValue)
	const float RangePctValue = FMath::GetRangePct(MinScaleDistance, MaxScalingDistance, Distance);

	// Clamp the result just in case of potential floating point inaccuracies slightly outside 0-1
	const float ClampedPctValue = FMath::Clamp(RangePctValue, 0.0f, 1.0f);

	// We want Alpha to be 1.0 when Distance is MinScaleDistance, and 0.0 when Distance is MaxScalingDistance.
	// Since ClampedPctValue is 0 at Min and 1 at Max, we subtract it from 1.0.
	const float Alpha = 1.0f - ClampedPctValue;

	// Linearly interpolate between full scale (1.0) and minimum scale (MinShipScaleFactor) using Alpha.
	// When Alpha is 1 (close to MinScaleDistance), result is near MinShipScaleFactor.
	// When Alpha is 0 (close to MaxScalingDistance), result is near 1.0.
	const float LerpedScale = FMath::Lerp(1.f, MinShipScaleFactor, Alpha);

	//UE_LOG(LogSolaraqSystem, Warning, TEXT("CalcScale Result: Lerp (MinShipScaleFactor: %.1f, MaxScalingDistance: %.1f, Alpha: %.1f) = %.2f"), MinScaleDistance, MaxScalingDistance, Distance/MaxScalingDistance, LerpedScale);
	return LerpedScale;
}

FVector ACelestialBodyBase::CalculateGravityForce(float Distance, const FVector& ShipLocation) const
{
    if (Distance < KINDA_SMALL_NUMBER || MaxInfluenceDistance <= 0.f) return FVector::ZeroVector; // Added check for valid influence distance

    const FVector ForceDirection = (GetActorLocation() - ShipLocation).GetSafeNormal();

    // Gravity calculation remains based on the main Influence radius
    const float DistanceRatio = FMath::Clamp(Distance / MaxInfluenceDistance, 0.0f, 1.0f);
    const float ForceMagnitude = GravitationalStrength * FMath::Pow(1.0f - DistanceRatio, GravityFalloffExponent);

    return ForceDirection * ForceMagnitude;
}

// --- UpdateInfluenceSphereRadius Helper Function ---
void ACelestialBodyBase::UpdateInfluenceSphereRadius()
{
    if (InfluenceSphereComponent)
    {
        // Set the component's radius from our property
        InfluenceSphereComponent->SetSphereRadius(InfluenceRadius);

        // Update the cached value used in calculations
        // Use GetScaledSphereRadius to account for potential actor scaling, though static actors usually have 1.0 scale.
        MaxInfluenceDistance = InfluenceSphereComponent->GetScaledSphereRadius();
    }
}

void ACelestialBodyBase::UpdateScalingSphereRadius()
{
    if (ScalingSphereComponent)
    {
        ScalingSphereComponent->SetSphereRadius(ScalingRadius);
        MaxScalingDistance = ScalingSphereComponent->GetScaledSphereRadius();
    }
}

void ACelestialBodyBase::ValidateRadii()
{
    // Ensure InfluenceRadius is non-negative
    if (InfluenceRadius < 0.0f)
    {
      //  UE_LOG(LogSolaraqCelestials, Warning, TEXT("CelestialBody '%s': InfluenceRadius %.1f is negative. Setting to 0."), *GetName(), InfluenceRadius);
        InfluenceRadius = 0.0f;
    }
    // Ensure ScalingRadius is non-negative
    if (ScalingRadius < 0.0f)
    {
      //  UE_LOG(LogSolaraqCelestials, Warning, TEXT("CelestialBody '%s': ScalingRadius %.1f is negative. Setting to 0."), *GetName(), ScalingRadius);
        ScalingRadius = 0.0f;
    }

    // Ensure Scaling Radius is not greater than Influence Radius
    if (ScalingRadius > InfluenceRadius)
    {
       // UE_LOG(LogSolaraqCelestials, Warning, TEXT("CelestialBody '%s': ScalingRadius (%.1f) > InfluenceRadius (%.1f). Clamping ScalingRadius."), *GetName(), ScalingRadius, InfluenceRadius);
        ScalingRadius = InfluenceRadius; // Clamp scaling radius to influence radius
    }

    // Optional: Ensure MinScaleDistance is less than ScalingRadius (Can also be done in PostEditChangeProperty)
    // if (MinScaleDistance >= ScalingRadius && ScalingRadius > 0)
    // {
    //     UE_LOG(LogSolaraqWarning, Warning, TEXT("CelestialBody '%s': MinScaleDistance (%.1f) >= ScalingRadius (%.1f). Clamping MinScaleDistance."), *GetName(), MinScaleDistance, ScalingRadius);
    //     MinScaleDistance = ScalingRadius * 0.95f;
    // }
}
