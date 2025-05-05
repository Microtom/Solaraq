#include "Environment/SolaraqSatellite.h"
#include "Components/StaticMeshComponent.h"
#include "Environment/CelestialBodyBase.h" // Include the full header here
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h" // For FMath
#include "Logging/SolaraqLogChannels.h" // Optional: Use your custom logging
#include "Kismet/KismetMathLibrary.h" // For FindLookAtRotation (optional)

ASolaraqSatellite::ASolaraqSatellite()
{
    // Set this actor to call Tick() every frame. Disable if not needed initially.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false; // Only tick if we have a valid body to orbit

    // Create the mesh component
    SatelliteMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SatelliteMesh"));
    SetRootComponent(SatelliteMeshComponent); // Mesh is the root
    SatelliteMeshComponent->SetMobility(EComponentMobility::Movable);
    SatelliteMeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName); // Make it collidable
    SatelliteMeshComponent->SetEnableGravity(false); // Doesn't use physics simulation directly

    // --- Replication Settings ---
    bReplicates = true;             // Enable Actor replication
    SetReplicateMovement(true);     // Automatically replicate position/rotation changes made on the server

    // Initial angle can be randomized slightly if desired
    CurrentOrbitAngle = FMath::FRandRange(0.0f, 360.0f);

    // Default values (can be overridden in editor)
    OrbitDistance = 5000.0f;
    OrbitSpeed = 10.0f;
    bClockwiseOrbit = true;
    GameplayPlaneZ = 0.0f; // Ensure this matches your desired gameplay Z-level

    UE_LOG(LogSolaraqGeneral, Verbose, TEXT("ASolaraqSatellite %s Constructed"), *GetName());
}

void ASolaraqSatellite::BeginPlay()
{
    Super::BeginPlay();

    // --- Server-Side Initialization ---
    if (HasAuthority())
    {
        if (IsValid(CelestialBodyToOrbit))
        {
            UE_LOG(LogSolaraqSystem, Log, TEXT("Satellite %s starting orbit around %s."), *GetName(), *CelestialBodyToOrbit->GetName());
            // Force initial calculation of projected center and position
            bRecalculateProjectedCenter = true;
            UpdateOrbitPosition(0.0f); // Calculate initial position without advancing angle
            SetActorTickEnabled(true); // Start ticking to move
        }
        else
        {
            UE_LOG(LogSolaraqSystem, Warning, TEXT("Satellite %s has no valid CelestialBodyToOrbit assigned. It will not orbit."), *GetName());
            SetActorTickEnabled(false); // Don't tick if no target
        }
    }
    // Clients will get the initial position via replication and OnRep_CelestialBodyToOrbit might trigger logic if needed
}

void ASolaraqSatellite::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Orbit logic only runs on the server
    if (HasAuthority())
    {
        UpdateOrbitPosition(DeltaTime);
    }
}

void ASolaraqSatellite::UpdateOrbitPosition(float DeltaTime)
{
    // This function runs only on the Server because Tick logic is guarded by HasAuthority()

    if (!IsValid(CelestialBodyToOrbit))
    {
        // Safety check - stop ticking if the body becomes invalid during gameplay
        if (IsActorTickEnabled())
        {
            UE_LOG(LogSolaraqSystem, Warning, TEXT("Satellite %s lost its CelestialBodyToOrbit. Stopping orbit."), *GetName());
            SetActorTickEnabled(false);
        }
        return;
    }

    // --- Calculate Projected Center ---
    // Optimization: Only recalculate if needed (e.g., first time, or if body could move)
    // For now, let's assume the celestial body is static after BeginPlay for simplicity.
    // If bodies can move, you'd need to check if CelestialBodyToOrbit->GetActorLocation() changed.
    if (bRecalculateProjectedCenter)
    {
        const FVector CenterLocation = CelestialBodyToOrbit->GetActorLocation();
        CachedProjectedCenter = FVector(CenterLocation.X, CenterLocation.Y, GameplayPlaneZ);
        bRecalculateProjectedCenter = false; // Reset flag
        UE_LOG(LogSolaraqSystem, Verbose, TEXT("Satellite %s: Recalculated Projected Center to %s"), *GetName(), *CachedProjectedCenter.ToString());
    }

    // --- Update Orbit Angle ---
    // Only advance angle if DeltaTime > 0 (i.e., not the initial placement)
    if (DeltaTime > 0.0f)
    {
        const float AngleDelta = (bClockwiseOrbit ? OrbitSpeed : -OrbitSpeed) * DeltaTime;
        CurrentOrbitAngle = FMath::Fmod(CurrentOrbitAngle + AngleDelta, 360.0f);
        if (CurrentOrbitAngle < 0.0f) // Ensure angle stays positive
        {
            CurrentOrbitAngle += 360.0f;
        }
    }

    // --- Calculate New Position ---
    const float AngleRad = FMath::DegreesToRadians(CurrentOrbitAngle);
    const float OffsetX = FMath::Cos(AngleRad) * OrbitDistance;
    const float OffsetY = FMath::Sin(AngleRad) * OrbitDistance;

    const FVector NewLocation = CachedProjectedCenter + FVector(OffsetX, OffsetY, 0.0f);

    // --- Apply Position ---
    // SetActorLocation should be sufficient as movement replication handles the rest.
    SetActorLocation(NewLocation);

    // --- Optional: Update Rotation ---
    // Make the satellite face outwards from the projected center point
    // const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(NewLocation, CachedProjectedCenter);
    // SetActorRotation(FRotator(0.0f, LookAtRotation.Yaw, 0.0f)); // Keep it level on XY plane

    // UE_LOG(LogSolaraqMovement, VeryVerbose, TEXT("Satellite %s: Angle=%.2f, Pos=%s"), *GetName(), CurrentOrbitAngle, *NewLocation.ToString());
}

void ASolaraqSatellite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Replicate the reference to the celestial body so clients know what they *should* be orbiting
    DOREPLIFETIME(ASolaraqSatellite, CelestialBodyToOrbit);

    // Note: We are NOT replicating OrbitDistance, OrbitSpeed, bClockwiseOrbit, CurrentOrbitAngle
    // because the server calculates the position and SetReplicateMovement handles sending the transform.
    // Clients don't *need* these variables unless you implement client-side prediction.
}

void ASolaraqSatellite::OnRep_CelestialBodyToOrbit()
{
    // This function executes on CLIENTS when CelestialBodyToOrbit is replicated.
    if (IsValid(CelestialBodyToOrbit))
    {
        UE_LOG(LogSolaraqSystem, Verbose, TEXT("Client %s: Received CelestialBodyToOrbit %s."), *GetName(), *CelestialBodyToOrbit->GetName());
        // Potential use: If movement replication lags significantly, you might force
        // a visual snap here based on a client-side calculation, but usually not needed.
        // Ensure tick is enabled/disabled based on the received body state
        // NOTE: Checking HasAuthority() is NOT needed here, this *only* runs on clients.
         if (!IsActorTickEnabled()) // If we weren't ticking (maybe body was null before)
         {
             // Clients don't NEED to tick for movement, but might need to for other effects?
             // SetActorTickEnabled(true); // Be cautious enabling tick on clients unnecessarily
         }
    }
    else
    {
         UE_LOG(LogSolaraqSystem, Verbose, TEXT("Client %s: CelestialBodyToOrbit became null."), *GetName());
         // SetActorTickEnabled(false); // Disable client tick if body is gone
    }

    // Force recalculation of projected center next time it's needed (e.g., if client logic uses it)
    bRecalculateProjectedCenter = true;
}


#if WITH_EDITOR
// --- Editor Helper Functions ---

void ASolaraqSatellite::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

    // If any orbit parameter changed, update the position in the editor view
    if (PropertyName == GET_MEMBER_NAME_CHECKED(ASolaraqSatellite, CelestialBodyToOrbit) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ASolaraqSatellite, OrbitDistance) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ASolaraqSatellite, GameplayPlaneZ))
        // OrbitSpeed/bClockwiseOrbit don't affect static position, CurrentOrbitAngle isn't editable
    {
        UpdatePositionInEditor();
    }
     // Validate OrbitDistance if changed
    if (PropertyName == GET_MEMBER_NAME_CHECKED(ASolaraqSatellite, OrbitDistance))
    {
        if(OrbitDistance < 0) OrbitDistance = 0; // Ensure non-negative
    }
}

void ASolaraqSatellite::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // Update position whenever the actor is moved or constructed in the editor
    UpdatePositionInEditor();
}

void ASolaraqSatellite::UpdatePositionInEditor()
{
    // Only run this logic if we are in the editor and not in PIE (Play In Editor)
    if (!GetWorld() || GetWorld()->IsGameWorld())
    {
        return; // Don't run during gameplay
    }

    if (IsValid(CelestialBodyToOrbit))
    {
        // Use current properties to place the satellite visually
        const FVector CenterLocation = CelestialBodyToOrbit->GetActorLocation();
        const FVector ProjectedCenter = FVector(CenterLocation.X, CenterLocation.Y, GameplayPlaneZ);

        // Use CurrentOrbitAngle (or 0 if you prefer a consistent editor preview)
        const float AngleRad = FMath::DegreesToRadians(CurrentOrbitAngle); // Or 0.0f
        const float OffsetX = FMath::Cos(AngleRad) * OrbitDistance;
        const float OffsetY = FMath::Sin(AngleRad) * OrbitDistance;

        const FVector NewLocation = ProjectedCenter + FVector(OffsetX, OffsetY, 0.0f);
        SetActorLocation(NewLocation);

        // Optional: Set rotation to look outwards
        // const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(NewLocation, ProjectedCenter);
        // SetActorRotation(FRotator(0.0f, LookAtRotation.Yaw, 0.0f));
    }
}
#endif // WITH_EDITOR