// ItemActor_FishingRod.cpp
#include "Items/Fishing/ItemActor_FishingRod.h"

// All necessary includes
#include "Items/Fishing/FishingBobber.h"
#include "Pawns/SolaraqCharacterPawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Items/ItemToolDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Logging/SolaraqLogChannels.h"
#include "Systems/FishingSubsystem.h"
#include "DrawDebugHelpers.h"

// --- CONSTRUCTOR ---
AItemActor_FishingRod::AItemActor_FishingRod()
{
    PrimaryActorTick.bCanEverTick = true; 

    if (DefaultSceneRoot)
    {
        DefaultSceneRoot->DestroyComponent();
        DefaultSceneRoot = nullptr;
    }
    RodMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RodMesh"));
    RootComponent = RodMesh;

    FishingLineMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FishingLineMesh"));
    FishingLineMesh->SetupAttachment(RootComponent);

    IdleBobberMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IdleBobberMesh"));
    IdleBobberMesh->SetupAttachment(RootComponent);
    IdleBobberMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AItemActor_FishingRod::BeginPlay()
{
    Super::BeginPlay();
    
    InitializeRope();
    bIsRopeInitialized = true;
}

// --- ON EQUIP ---
void AItemActor_FishingRod::OnEquip()
{
    Super::OnEquip();
}

// --- ON ITEM DATA CHANGED ---
void AItemActor_FishingRod::OnItemDataChanged()
{
    Super::OnItemDataChanged();

    if (const UItemToolDataAsset* ToolData = Cast<UItemToolDataAsset>(ItemData))
    {
        if (ToolData->ToolSkeletalMesh) RodMesh->SetSkeletalMesh(ToolData->ToolSkeletalMesh);
        if (ToolData->ToolAnimClass) RodMesh->SetAnimInstanceClass(ToolData->ToolAnimClass);
    }

    if (BobberClass)
    {
        if (const AFishingBobber* DefaultBobber = BobberClass.GetDefaultObject())
        {
            if (DefaultBobber->MeshComponent)
            {
                IdleBobberMesh->SetStaticMesh(DefaultBobber->MeshComponent->GetStaticMesh());
            }
        }
    }
}

// --- TICK ---
void AItemActor_FishingRod::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Add this safety check
    if (!bIsRopeInitialized)
    {
        return;
    }
    
    // This is the sub-stepping loop
    TimeAccumulator += DeltaSeconds;
    while (TimeAccumulator >= TimeStep)
    {
        // We have enough accumulated time to run a full simulation step
        UpdateRopeLength(TimeStep); // Update length using the fixed time step
        SimulateRope(TimeStep);     // Simulate using the fixed time step
        
        TimeAccumulator -= TimeStep; // Subtract the time we just used
    }
    
    DrawRope();                     //  Render the result
}


// --- ROPE SIMULATION & DRAWING ---

void AItemActor_FishingRod::InitializeRope()
{
    // Make sure the initial length is valid.
    if (InitialRopeLength <= 0.0f || RopeSegmentLength <= 0.0f)
    {
        // Fallback to the minimal 2-particle, zero-length rope if properties are invalid.
        RopeParticles.Empty();
        const FVector RodTipLocation = RodMesh->GetSocketLocation(RodTipSocketName);
        RopeParticles.Add({ RodTipLocation, RodTipLocation });
        RopeParticles.Add({ RodTipLocation, RodTipLocation });
        CurrentRopeLength = 0.0f;
        TargetRopeLength = 0.0f;
        return;
    }

    // --- NEW INITIALIZATION LOGIC ---
    RopeParticles.Empty();
    const int32 NumParticles = FMath::CeilToInt(InitialRopeLength / RopeSegmentLength) + 1;
    const FVector RodTipLocation = RodMesh->GetSocketLocation(RodTipSocketName);

    for (int32 i = 0; i < NumParticles; ++i)
    {
        // Calculate the length of this specific particle's position along the rope
        const float LengthAtParticle = FMath::Min((float)i * RopeSegmentLength, InitialRopeLength);

        FVerletParticle Particle;
        // Create the particles in a straight line hanging down from the tip
        Particle.Position = RodTipLocation - FVector(0, 0, LengthAtParticle);
        Particle.OldPosition = Particle.Position; // Start at rest
        RopeParticles.Add(Particle);
    }

    // Set the initial lengths to match the physical state
    CurrentRopeLength = InitialRopeLength;
    TargetRopeLength = InitialRopeLength;
}

void AItemActor_FishingRod::SimulateRope(float DeltaTime)
{
    if (RopeParticles.Num() < 2) return;

    const float Friction = 0.2f;      // How much velocity is lost when sliding. 0 = no friction, 1 = max friction.
    const float Bounce = 0.1f;
    
    const FVector Gravity = FVector(0, 0, GetWorld()->GetGravityZ());
    for (int32 i = 1; i < RopeParticles.Num(); ++i)
    {
        FVector& Position = RopeParticles[i].Position;
        FVector& OldPosition = RopeParticles[i].OldPosition;
        
        // Calculate velocity, apply damping, then calculate new position
        const FVector Velocity = (Position - OldPosition) * Damping; // Apply damping here
        OldPosition = Position;
        Position += Velocity + (Gravity * DeltaTime * DeltaTime);
    }
    
    
    for (int32 i = 0; i < RopeSolverIterations; ++i)
    {
        RopeParticles[0].Position = RodMesh->GetSocketLocation(RodTipSocketName);

        // **THE KEY CHANGE** - Handle the first, partially extruded segment
        const float NumFullSegments = RopeParticles.Num() - 2;
        const float FirstSegmentLength = CurrentRopeLength - (NumFullSegments * RopeSegmentLength);

        // Apply constraint for the first segment
        FVector& ParticleA_Pos = RopeParticles[0].Position;
        FVector& ParticleB_Pos = RopeParticles[1].Position;
        FVector Delta = ParticleB_Pos - ParticleA_Pos;
        float Error = Delta.Size() - FirstSegmentLength;
        FVector ChangeDir = Delta.GetSafeNormal();
        ParticleB_Pos -= ChangeDir * Error; // Only move the second particle

        // Apply constraints for all other full-length segments
        for (int32 j = 1; j < RopeParticles.Num() - 1; ++j)
        {
            FVector& SegA = RopeParticles[j].Position;
            FVector& SegB = RopeParticles[j + 1].Position;

            Delta = SegB - SegA;
            Error = Delta.Size() - RopeSegmentLength;
            ChangeDir = Delta.GetSafeNormal();
            FVector ChangeAmount = ChangeDir * Error * 0.5f;
            
            SegA += ChangeAmount;
            SegB -= ChangeAmount;
        }
    }

    // STEP 3: COLLISION & RESPONSE (Apply world collision as the FINAL step)
    for (int32 i = 1; i < RopeParticles.Num(); ++i)
    {
        FVector& Position = RopeParticles[i].Position;
        FVector& OldPosition = RopeParticles[i].OldPosition;

        FHitResult HitResult;
        bool bHit = UKismetSystemLibrary::LineTraceSingle(
            GetWorld(),
            OldPosition,
            Position,
            UEngineTypes::ConvertToTraceType(ECC_WorldStatic),
            false, { this }, EDrawDebugTrace::None, HitResult, true
        );

        if (bHit)
        {
            const float DepenetrationOffset = 0.1f;
            const FVector DepenetrationVector = HitResult.ImpactNormal * DepenetrationOffset;

            const FVector ImpactVelocity = Position - OldPosition;
            const FVector ImpactNormal = HitResult.ImpactNormal;
            
            const FVector NormalComponent = ImpactNormal * FVector::DotProduct(ImpactVelocity, ImpactNormal);
            const FVector TangentComponent = ImpactVelocity - NormalComponent;

            Position = HitResult.ImpactPoint 
                     + DepenetrationVector 
                     + (TangentComponent * (1.0f - Friction))
                     - (NormalComponent * Bounce);
            
            OldPosition = HitResult.ImpactPoint + DepenetrationVector;
        }
    }
}

void AItemActor_FishingRod::UpdateRopeLength(float DeltaTime)
{
    // 1. Adjust the TargetRopeLength based on input
    if (bIsCasting)
    {
        TargetRopeLength += CastingSpeed * DeltaTime;
    }
    if (bIsReeling)
    {
        TargetRopeLength -= ReelSpeed * DeltaTime;
    }

    TargetRopeLength = FMath::Clamp(TargetRopeLength, 0.0f, MaxRopeLength);
    CurrentRopeLength = TargetRopeLength;

    // 2. Calculate Required Particles
    const int32 RequiredNumParticles = FMath::CeilToInt(CurrentRopeLength / RopeSegmentLength) + 1;
    const int32 CurrentNumParticles = RopeParticles.Num();

    // UE_LOG(LogSolaraqFishing, Warning, TEXT("UpdateRopeLength: Required=%d, Current=%d, Length=%.2f"), RequiredNumParticles, CurrentNumParticles, CurrentRopeLength);

    // 3. Add new particles if needed
    if (CurrentNumParticles < RequiredNumParticles)
    {
        // --- ISOLATED AND LOGGED ADD OPERATION ---
        if (CurrentNumParticles > 0)
        {
            // Get the last particle BEFORE we modify the array
            const FVerletParticle LastParticle = RopeParticles.Last();
            
            // Loop to add the necessary number of particles
            for (int32 i = CurrentNumParticles; i < RequiredNumParticles; ++i)
            {
                // Add a copy of the last known good particle
                RopeParticles.Add(LastParticle);
                // UE_LOG(LogSolaraqFishing, Warning, TEXT("... Added particle. New count: %d"), RopeParticles.Num());
            }
        }
        else // This case should not happen because of BeginPlay, but it's a good safety net.
        {
            UE_LOG(LogSolaraqFishing, Error, TEXT("CRITICAL: UpdateRopeLength trying to add particles to an empty array!"));
            // If the array is empty, we must re-initialize it to prevent a crash.
            InitializeRope();
            return; // Exit this frame's update to be safe.
        }
        // --- END OF ISOLATED OPERATION ---
    }
    // 4. Remove excess particles if needed
    else if (CurrentNumParticles > RequiredNumParticles)
    {
        // We ensure we never shrink below our minimum of 2 particles.
        const int32 NumToRemove = FMath::Min(CurrentNumParticles - RequiredNumParticles, CurrentNumParticles - 2);
        if (NumToRemove > 0)
        {
            const int32 IndexToRemoveFrom = CurrentNumParticles - NumToRemove;
            RopeParticles.RemoveAt(IndexToRemoveFrom, NumToRemove, EAllowShrinking::No);
        }
    }
}

void AItemActor_FishingRod::DrawRope()
{
    if (RopeParticles.Num() < 2)
    {
        FishingLineMesh->ClearAllMeshSections();
        IdleBobberMesh->SetVisibility(false);
        return;
    }
    
    IdleBobberMesh->SetVisibility(true);

    Vertices.Reset();
    Triangles.Reset();
    Normals.Reset();
    UVs.Reset();
    Tangents.Reset();

    const FTransform ComponentTransform = FishingLineMesh->GetComponentTransform();
    
    for (int32 i = 0; i < RopeParticles.Num() - 1; i++)
    {
        const FVector& StartPoint = RopeParticles[i].Position;
        const FVector& EndPoint = RopeParticles[i+1].Position;

        const FVector SegmentDirection = (EndPoint - StartPoint).GetSafeNormal();
        FVector UpVector = FVector::UpVector;
        if (FMath::Abs(FVector::DotProduct(SegmentDirection, UpVector)) > 0.99f)
        {
            UpVector = FVector::RightVector;
        }
        const FVector RightVector = FVector::CrossProduct(SegmentDirection, UpVector).GetSafeNormal();
        UpVector = FVector::CrossProduct(RightVector, SegmentDirection).GetSafeNormal();

        const int32 RingStartIndex = Vertices.Num();
        for (int32 j = 0; j < RopeSides; j++)
        {
            const float Angle = (float)j / (float)RopeSides * 2.0f * PI;
            const FVector Offset = (UpVector * FMath::Sin(Angle) + RightVector * FMath::Cos(Angle)) * RopeWidth;
            const FVector Normal = Offset.GetSafeNormal();

            // Calculate the vertex positions in WORLD space first.
            const FVector WorldPos_Start = StartPoint + Offset;
            const FVector WorldPos_End = EndPoint + Offset;

            // NOW, convert the world positions into the component's LOCAL space before adding them.
            Vertices.Add(ComponentTransform.InverseTransformPosition(WorldPos_Start));
            Normals.Add(Normal);
            UVs.Add(FVector2D((float)i / (RopeParticles.Num() - 1), (float)j / RopeSides));

            Vertices.Add(ComponentTransform.InverseTransformPosition(WorldPos_End));
            Normals.Add(Normal);
            UVs.Add(FVector2D((float)(i + 1) / (RopeParticles.Num() - 1), (float)j / RopeSides));
        }

        for (int32 j = 0; j < RopeSides; j++)
        {
            const int32 NextJ = (j + 1) % RopeSides;
            const int32 V0 = RingStartIndex + j * 2;
            const int32 V1 = RingStartIndex + NextJ * 2;
            const int32 V2 = RingStartIndex + j * 2 + 1;
            const int32 V3 = RingStartIndex + NextJ * 2 + 1;

            Triangles.Add(V0);
            Triangles.Add(V1);
            Triangles.Add(V2);
            Triangles.Add(V2);
            Triangles.Add(V1);
            Triangles.Add(V3);
        }
    }

    FishingLineMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, false);
    FishingLineMesh->SetMaterial(0, FishingLineMaterial);

    if (IdleBobberMesh)
    {
        IdleBobberMesh->SetWorldLocation(RopeParticles.Last().Position);
        if (RopeParticles.Num() > 1)
        {
            FVector LastSegmentDir = RopeParticles.Last().Position - RopeParticles[RopeParticles.Num() - 2].Position;
            IdleBobberMesh->SetWorldRotation(LastSegmentDir.Rotation());
        }
    }
}


// --- STUB IMPLEMENTATIONS TO SATISFY THE LINKER ---

void AItemActor_FishingRod::OnUnequip()
{
    Super::OnUnequip();

    if (UWorld* World = GetWorld())
    {
        if (UFishingSubsystem* FishingSubsystem = World->GetSubsystem<UFishingSubsystem>())
        {
            FishingSubsystem->OnToolUnequipped(this);
        }
    }
}

void AItemActor_FishingRod::PrimaryUse()
{
    if (UWorld* World = GetWorld())
    {
        if (UFishingSubsystem* FishingSubsystem = World->GetSubsystem<UFishingSubsystem>())
        {
            FishingSubsystem->RequestPrimaryAction(OwningPawn, this);
        }
    }
}

void AItemActor_FishingRod::PrimaryUse_Stop()
{
    if (UWorld* World = GetWorld())
    {
        if (UFishingSubsystem* FishingSubsystem = World->GetSubsystem<UFishingSubsystem>())
        {
            FishingSubsystem->RequestPrimaryAction_Stop(OwningPawn, this);
        }
    }
}

AFishingBobber* AItemActor_FishingRod::SpawnAndCastBobber(const FVector& CastDirection, float Charge)
{
    UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): 'SpawnAndCastBobber' called. Starting to extend line."), *GetName());

    // --- NEW: Calculate the angled launch direction ---

    // 1. Get the axis to rotate around. This is the vector perpendicular to the aim direction and the world up vector.
    //    For a forward aim (1,0,0), this axis will be (0,1,0) (the Y-axis).
    const FVector RotationAxis = FVector::CrossProduct(CastDirection, FVector::UpVector).GetSafeNormal();

    // 2. Rotate the horizontal direction vector upwards by our CastAngle.
    const FVector LaunchDirection = CastDirection.RotateAngleAxis(CastAngle, RotationAxis);

    // --- NEW: Draw Debug Line ---
    const FVector RodTipLocation = RodMesh->GetSocketLocation(RodTipSocketName);
    DrawDebugLine(
        GetWorld(),
        RodTipLocation,
        RodTipLocation + LaunchDirection * 500.f, // Draw a 5m line in the launch direction
        FColor::Green,
        false, // Not persistent
        30.0f,  // Lasts for 5 seconds
        0,
        10.f   // Thickness
    );

    const float CastSpeed = FMath::Lerp(500.f, 2000.f, Charge);
    // Use the new angled LaunchDirection for the velocity
    const FVector InitialVelocity = LaunchDirection * CastSpeed;

    for (FVerletParticle& Particle : RopeParticles)
    {
        // Use Verlet integration to impart a velocity: V = (Pos - OldPos)
        // So, OldPos = Pos - V * DeltaT
        Particle.OldPosition = Particle.Position - (InitialVelocity * TimeStep);
    }

    bIsCasting = true;
    bIsReeling = false;

    return nullptr;
}

void AItemActor_FishingRod::StartReeling()
{
    UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): StartReeling() called."), *GetName());
    bIsCasting = false;
    bIsReeling = true;
}

void AItemActor_FishingRod::NotifyFishBite()
{
    UE_LOG(LogSolaraqFishing, Warning, TEXT("Rod (%s): NotifyFishBite() called (stub)."), *GetName());
    if (FishBiteSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, FishBiteSound, GetActorLocation());
    }

    if (RopeParticles.Num() > 0)
    {
        // Apply a sharp downward impulse to the last particle (the bobber)
        // We do this by directly modifying its OldPosition to simulate instant velocity.
        FVerletParticle& LastParticle = RopeParticles.Last();
        LastParticle.OldPosition.Z += 25.0f; // This creates a downward velocity on the next simulation step.
    }
}

void AItemActor_FishingRod::NotifyReset()
{
    UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): NotifyReset() called."), *GetName());
    bIsCasting = false;
    bIsReeling = false;
    InitializeRope(); // Reset the rope to its initial hanging state.
    if(CurrentBobber)
    {
        CurrentBobber->Destroy();
        CurrentBobber = nullptr;
    }
}