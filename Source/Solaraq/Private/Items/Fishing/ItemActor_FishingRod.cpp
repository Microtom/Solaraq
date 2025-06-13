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

    #define ECC_FishingLine ECC_GameTraceChannel1
    FishingLineMesh->SetCollisionObjectType(ECC_FishingLine);
    FishingLineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
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

    if (CurrentBobber && bBobberHasLanded)
    {
        CurrentBobber->SetActorLocation(RopeParticles.Last().Position);
    }
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
    // --- PRE-SIMULATION CHECKS ---
    if (RopeParticles.Num() < 2)
    {
        return;
    }

    if (GetWorld() == nullptr)
    {
        UE_LOG(LogSolaraqFishing, Error, TEXT("Rod (%s): SimulateRope - GetWorld() is NULL! Aborting simulation."), *GetName());
        return;
    }

    const FVector Gravity = FVector(0, 0, GetWorld()->GetGravityZ());
    const FVector RodTipLocation = RodMesh->GetSocketLocation(RodTipSocketName);

    // Check if the bobber is currently in the air (flying)
    if (CurrentBobber && !bBobberHasLanded)
    {
        // --- IN-AIR LOGIC (Bobber is flying) ---
        // Use a quadratic Bezier curve for a stable and visually pleasing arc that respects the ground.
        
        const FVector P0 = RodTipLocation;                              // Start Point
        const FVector P2 = CurrentBobber->GetActorLocation();           // End Point

        // Calculate the control point (P1) to create the arc
        const FVector MidPoint = (P0 + P2) * 0.5f;
        const float SagMagnitude = FVector::Dist(P0, P2) * 0.15f; // Tweak for more/less sag
        const float WindSway = FMath::Sin(GetWorld()->GetTimeSeconds() * 2.0f) * 20.0f; // Tweak for wind effect
        const FVector P1 = MidPoint + FVector(0, 0, -SagMagnitude) + FVector(0, WindSway, 0);

        for (int32 i = 0; i < RopeParticles.Num(); ++i)
        {
            // Position particles along the curve
            const float Alpha = (float)i / (float)(RopeParticles.Num() - 1);
            const float OneMinusAlpha = 1.0f - Alpha;

            FVector ParticlePosition = 
                (OneMinusAlpha * OneMinusAlpha * P0) +
                (2.0f * OneMinusAlpha * Alpha * P1) +
                (Alpha * Alpha * P2);

            // Ground Collision Check for the in-air line
            FHitResult HitResult;
            const FVector TraceStart = FVector(ParticlePosition.X, ParticlePosition.Y, ParticlePosition.Z + 200.0f);
            const FVector TraceEnd = FVector(ParticlePosition.X, ParticlePosition.Y, ParticlePosition.Z - 200.0f);
            
            if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic) && ParticlePosition.Z < HitResult.ImpactPoint.Z)
            {
                ParticlePosition.Z = HitResult.ImpactPoint.Z;
            }

            RopeParticles[i].Position = ParticlePosition;
            RopeParticles[i].OldPosition = ParticlePosition;
        }
    }
    else
    {
        // --- AT-REST / REELING LOGIC (Bobber has landed or is being reeled) ---
        // Use a Verlet integration simulation that includes gravity and collision for all particles.

        // STEP 1: VERLET INTEGRATION (APPLY GRAVITY & DAMPING)
        // This loop now includes the last particle, re-applying gravity to our simulated bobber.
        for (int32 i = 1; i < RopeParticles.Num(); ++i)
        {
            FVector& Position = RopeParticles[i].Position;
            FVector& OldPosition = RopeParticles[i].OldPosition;
            
            const FVector Velocity = (Position - OldPosition) * Damping;
            OldPosition = Position;
            Position += Velocity + (Gravity * DeltaTime * DeltaTime);
        }
        
        // STEP 2: CONSTRAINT SOLVING (MAINTAIN ROPE LENGTH)
        for (int32 Iteration = 0; Iteration < RopeSolverIterations; ++Iteration)
        {
            // Anchor the first particle to the rod tip
            RopeParticles[0].Position = RodTipLocation;

            // Solve partial first segment (if reeling)
            if (RopeParticles.Num() > 1)
            {
                const float NumFullSegments = RopeParticles.Num() > 2 ? RopeParticles.Num() - 2 : 0;
                const float FirstSegmentLength = CurrentRopeLength - (NumFullSegments * RopeSegmentLength);

                FVector& ParticleA_Pos = RopeParticles[0].Position;
                FVector& ParticleB_Pos = RopeParticles[1].Position;
                FVector Delta = ParticleB_Pos - ParticleA_Pos;
                float CurrentDistance = Delta.Size();
                float Error = CurrentDistance - FirstSegmentLength;

                if (Error > 0) // Only pull, never push
                {
                    FVector ChangeDir = (CurrentDistance > KINDA_SMALL_NUMBER) ? Delta / CurrentDistance : FVector(0,0,-1);
                    ParticleB_Pos -= ChangeDir * Error;
                }
            }
            
            // Solve full-length segments
            for (int32 j = 1; j < RopeParticles.Num() - 1; ++j)
            {
                FVector& SegA = RopeParticles[j].Position;
                FVector& SegB = RopeParticles[j + 1].Position;

                FVector Delta = SegB - SegA;
                float CurrentDistance = Delta.Size();
                float Error = CurrentDistance - RopeSegmentLength;

                if (Error > 0) // Only pull, never push
                {
                    FVector ChangeDir = (CurrentDistance > KINDA_SMALL_NUMBER) ? Delta / CurrentDistance : FVector(0,0,-1);
                    FVector ChangeAmount = ChangeDir * Error * 0.5f;
                
                    SegA += ChangeAmount;
                    SegB -= ChangeAmount;
                }
            }
        }

        // STEP 3: COLLISION & RESPONSE (APPLY WORLD COLLISION)
        // This loop also includes the last particle, ensuring the bobber is dragged along the ground.
        const float Friction = 0.2f;
        const float Bounce = 0.1f;
        for (int32 i = 1; i < RopeParticles.Num(); ++i)
        {
            FVector& Position = RopeParticles[i].Position;
            FVector& OldPosition = RopeParticles[i].OldPosition;

            FHitResult HitResult;
            if (GetWorld()->LineTraceSingleByChannel(HitResult, OldPosition, Position, ECC_WorldStatic))
            {
                const FVector DepenetrationVector = HitResult.ImpactNormal * 0.1f;
                const FVector ImpactVelocity = Position - OldPosition;
                const FVector NormalComponent = HitResult.ImpactNormal * FVector::DotProduct(ImpactVelocity, HitResult.ImpactNormal);
                const FVector TangentComponent = ImpactVelocity - NormalComponent;

                Position = HitResult.ImpactPoint + DepenetrationVector + (TangentComponent * (1.0f - Friction)) - (NormalComponent * Bounce);
                OldPosition = HitResult.ImpactPoint + DepenetrationVector;
            }
        }
    }
}

void AItemActor_FishingRod::UpdateRopeLength(float DeltaTime)
{
    
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

AFishingBobber* AItemActor_FishingRod::SpawnAndCastBobber(const FVector& HorizontalCastDirection, float Charge)
{
    // --- PRE-CAST CHECKS & LOGGING ---
    UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): 'SpawnAndCastBobber' called. Charge: %.2f"), *GetName(), Charge);

    if (!BobberClass)
    {
        UE_LOG(LogSolaraqFishing, Error, TEXT("Rod (%s): BobberClass is not set! Cannot cast."), *GetName());
        return nullptr;
    }

    if (GetWorld() == nullptr)
    {
        UE_LOG(LogSolaraqFishing, Error, TEXT("Rod (%s): GetWorld() is NULL! Cannot cast."), *GetName());
        return nullptr;
    }

    // --- CLEANUP ---
    if (CurrentBobber)
    {
        UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): Destroying previous bobber."), *GetName());
        CurrentBobber->Destroy();
        CurrentBobber = nullptr;
    }

    // --- ROPE SIZING ---
    TargetRopeLength = FMath::Lerp(MinCastRopeLength, MaxRopeLength, Charge);
    UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): Calculated TargetRopeLength: %.2f"), *GetName(), TargetRopeLength);
    
    UpdateRopeLength(0.0f); // Immediately resize the particle array
    UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): Rope particle array resized to %d particles."), *GetName(), RopeParticles.Num());

    // --- CAST CALCULATION ---
    const FVector RodTipLocation = RodMesh->GetSocketLocation(RodTipSocketName);
    const float CastSpeed = FMath::Lerp(400.f, 1380.f, Charge);
    const FVector LaunchDirection = HorizontalCastDirection.RotateAngleAxis(CastAngle, FVector::CrossProduct(HorizontalCastDirection, FVector::UpVector).GetSafeNormal());
    UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): LaunchDirection: %s, CastSpeed: %.2f"), *GetName(), *LaunchDirection.ToString(), CastSpeed);

    // --- BOBBER SPAWNING ---
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = GetInstigator();
    CurrentBobber = GetWorld()->SpawnActor<AFishingBobber>(BobberClass, RodTipLocation, LaunchDirection.Rotation(), SpawnParams);
    
    if (!CurrentBobber)
    {
        UE_LOG(LogSolaraqFishing, Error, TEXT("Rod (%s): FAILED to spawn Bobber actor!"), *GetName());
        return nullptr;
    }
    UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): Successfully spawned bobber: %s"), *GetName(), *CurrentBobber->GetName());
    
    // Give the spawned bobber its launch velocity
    CurrentBobber->ProjectileMovement->Velocity = LaunchDirection * CastSpeed;

    // --- ROPE INITIALIZATION (THE FIX) ---
    // Instead of piling the particles at the tip, we lay them out in a straight line along the launch direction.
    // This represents the rope being shot out of the reel.
    if (RopeParticles.Num() > 1)
    {
        UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): Initializing rope particle positions along launch vector."), *GetName());
        for (int32 i = 0; i < RopeParticles.Num(); ++i)
        {
            // Calculate how far along the total length this particle should be
            const float LengthAlongRope = ((float)i / (float)(RopeParticles.Num() - 1)) * TargetRopeLength;
            const FVector ParticlePosition = RodTipLocation + LaunchDirection * LengthAlongRope;

            RopeParticles[i].Position = ParticlePosition;
            RopeParticles[i].OldPosition = ParticlePosition; // Start at rest (no initial velocity for the rope itself)
        }
    }
    
    // --- FINAL STATE SETUP ---
    bIsCasting = false;
    bIsReeling = false;
    bBobberHasLanded = false;

    IdleBobberMesh->SetVisibility(false); // Hide the placeholder mesh on the rod
    
    UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): SpawnAndCastBobber finished successfully."), *GetName());
    return CurrentBobber;
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
    bBobberHasLanded = false;
    InitializeRope(); // Reset the rope to its initial hanging state.
    if(CurrentBobber)
    {
        CurrentBobber->Destroy();
        CurrentBobber = nullptr;
    }
}

void AItemActor_FishingRod::StopReeling()
{
    UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): StopReeling() called."), *GetName());
    bIsReeling = false;
}

void AItemActor_FishingRod::NotifyBobberLanded()
{
    if (bBobberHasLanded) return; // Prevent this from running more than once per cast

    UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): NotifyBobberLanded() called. Freezing rope length."), *GetName());
    bBobberHasLanded = true;

    // The key change: The final length of the rope is now locked to its current physical length.
    TargetRopeLength = CurrentRopeLength;
}
