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

DEFINE_LOG_CATEGORY(LogSolaraqPhysics);

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

void AItemActor_FishingRod::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 1. Anchor Rod Tip (Particle 0) - Always pinned to the bone
    if (RopeSolver.Particles.Num() > 0)
    {
        FVector RodTip = RodMesh->GetSocketLocation(RodTipSocketName);
        RopeSolver.Particles[0].Position = RodTip;
        RopeSolver.Particles[0].PrevPosition = RodTip; 
        RopeSolver.Particles[0].InverseMass = 0.0f;   
    }

    // 2. Logic (Unspooling/Reeling)
    ManageRopeState(DeltaSeconds);

    // 3. Anchor/Update Bobber (Last Particle)
    if (CurrentBobber && RopeSolver.Particles.Num() > 1)
    {
        FXPBDParticle& LastP = RopeSolver.Particles.Last();
        
        if (!bBobberHasLanded)
        {
            // STATE: CASTING / FLYING
            // The Bobber Actor (Projectile) drives the Rope.
            // The particle is effectively "Parented" to the actor.
            LastP.Position = CurrentBobber->GetActorLocation();
            LastP.PrevPosition = LastP.Position; 
            LastP.InverseMass = 0.0f; // Infinite mass, immovable by rope physics
        }
        else
        {
            // LANDED
            float BobberMass = RopeMass * 5.0f;
            LastP.InverseMass = (BobberMass > 0.f) ? 1.0f / BobberMass : 1.0f; 
            
            // --- FIX START: Dynamic Friction ---
            if (IsReeling())
            {
                // REELING: Low Drag (Slippery)
                // This allows the rope to pull the bobber across the ground
                LastP.Velocity *= 0.95f; 
                
                // Do NOT force velocity to zero. Let it slide.
            }
            else
            {
                // IDLE/WAITING: High Drag (Sticky/Stable)
                // This prevents the jitter/drift when just waiting
                if (LastP.Velocity.SizeSquared() < 100.0f) 
                {
                    LastP.Velocity = FVector::ZeroVector;
                    LastP.PrevPosition = LastP.Position; 
                }
                else
                {
                    LastP.Velocity *= 0.8f; 
                }
            }
        }
    }

    // 4. Run Physics Solver
    RopeSolver.SubSteps = SolverSubSteps;
    RopeSolver.Simulate(DeltaSeconds, GetWorld());

    // 5. POST-SOLVE: Sync Bobber Actor to Rope
    if (CurrentBobber && bBobberHasLanded && RopeSolver.Particles.Num() > 1)
    {
        FXPBDParticle& LastP = RopeSolver.Particles.Last();
        FVector DesiredPos = LastP.Position;
        FVector CurrentPos = CurrentBobber->GetActorLocation();

        FVector CurrentActorPos = CurrentBobber->GetActorLocation();
        
        // Only update if there is a meaningful difference (optimization + stability)
        if (!DesiredPos.Equals(CurrentPos, 0.5f))
        {
            FHitResult Hit;
            // Sweep the actor to the particle's position. 
            // The 'true' flag ensures we collide with rocks/ground.
            bool bHit = CurrentBobber->SetActorLocation(DesiredPos, true, &Hit);

            if (bHit)
            {
                // We hit something (e.g., pulled against a rock).
                // The Actor stopped short of the Particle.
                // We must snap the Particle back to the Actor's actual location.
                // If we don't, the particle stays inside the rock and pulls the rope weirdly.
                LastP.Position = CurrentBobber->GetActorLocation();
                
                // Kill momentum into the wall so it doesn't keep banging against it
                LastP.PrevPosition = LastP.Position; 
            }
        }
    }

    // 6. Draw
    // Optional: Only draw if you want debug lines for constraints
    // RopeSolver.DebugDraw(GetWorld()); 
    DrawRope();
}

// --- ROPE SIMULATION & DRAWING ---

void AItemActor_FishingRod::InitializeRope()
{
    UE_LOG(LogSolaraqPhysics, Log, TEXT("InitializeRope Called."));
    const FVector Tip = RodMesh->GetSocketLocation(RodTipSocketName);
    
    // Minimal initialization: Just a small hanging rope
    TArray<FVector> InitialPoints;
    InitialPoints.Add(Tip);
    InitialPoints.Add(Tip - FVector(0,0,10)); 

    RopeSolver.Initialize(InitialPoints, RopeMass);
    CurrentRopeLength = 10.0f;
    TargetRopeLength = 10.0f;
}

void AItemActor_FishingRod::ManageRopeState(float DeltaTime)
{
    if (RopeSolver.Particles.Num() < 2) return;

    // Sync our public variable with the Physics Engine's truth
    CurrentRopeLength = RopeSolver.GetTotalRestLength();

    // --- UNSPOOLING (CASTING) ---
    if (bIsCasting && !bIsReeling)
    {
        // 1. Check Limits
        if (CurrentRopeLength >= TargetRopeLength)
        {
            bIsCasting = false; 
            return;
        }
        
        // 2. Unspooling Logic
        FVector RodTip = RopeSolver.Particles[0].Position;
        
        // LOOP to catch up with fast moving bobbers
        int32 SafetyLoop = 0;
        float DistToNext = FVector::Dist(RodTip, RopeSolver.Particles[1].Position);

        while (DistToNext > RopeSegmentLength && SafetyLoop < 10)
        {
            // Insert new particle at the ideal segment distance
            FVector Direction = (RopeSolver.Particles[1].Position - RodTip).GetSafeNormal();
            FVector NewPos = RodTip + (Direction * RopeSegmentLength); // Place it perfectly
            
            RopeSolver.InsertParticleAfterHead(NewPos, RopeMass, RopeSolver.BaseCompliance);
            
            // Recalculate distance for next iteration (Particle 1 is now the NEW particle)
            DistToNext = FVector::Dist(RodTip, RopeSolver.Particles[1].Position);
            SafetyLoop++;
        }
    }
    
    // --- REELING ---
    UFishingSubsystem* FishingSS = GetWorld()->GetSubsystem<UFishingSubsystem>();
    bool bFishPulling = (FishingSS && FishingSS->IsFishPulling());

    if (bIsReeling && !bIsCasting)
    {
        if (RopeSolver.Constraints.Num() > 0)
        {
            FXPBDConstraint& FirstConstraint = RopeSolver.Constraints[0];
            
            // Reel in
            float AmountToReel = ReelSpeed * DeltaTime;
            FirstConstraint.RestLength -= AmountToReel;

            // Delete particle if consumed
            if (FirstConstraint.RestLength <= 1.0f) 
            {
                // Capture the distance to the *next* particle before we delete the current one
                // This ensures smooth visual transition
                float DistToNext = 0.0f;
                if(RopeSolver.Particles.Num() > 2)
                {
                    DistToNext = FVector::Dist(RopeSolver.Particles[1].Position, RopeSolver.Particles[2].Position);
                }

                RopeSolver.RemoveParticleAfterHead(RopeSolver.BaseCompliance);
                
                // Set the new first constraint to match physical reality to prevent popping
                if (RopeSolver.Constraints.Num() > 0 && DistToNext > 0.0f)
                {
                    RopeSolver.Constraints[0].RestLength = DistToNext;
                }
            }
        }
    }
    
    // --- FISH PULLING ---
    if (bFishPulling && !bIsReeling)
    {
         // Same logic: simply extend the first constraint
         // The physics engine handles the "pull" because the constraints are stiff
         if (RopeSolver.Constraints.Num() > 0)
         {
            RopeSolver.Constraints[0].RestLength += FishPullSpeed * DeltaTime;
            
            // If constraint gets too long, split it (Unspool)
            if (RopeSolver.Constraints[0].RestLength > RopeSegmentLength * 1.5f)
            {
                 FVector RodTip = RopeSolver.Particles[0].Position;
                 FVector NextP = RopeSolver.Particles[1].Position;
                 FVector NewPos = FMath::Lerp(RodTip, NextP, 0.5f);
                 RopeSolver.InsertParticleAfterHead(NewPos, RopeMass, RopeSolver.BaseCompliance);
            }
         }
    }
}

void AItemActor_FishingRod::DrawRope()
{
    // 1. Basic Safety: Do we have enough particles to draw a line?
    if (RopeSolver.Particles.Num() < 2)
    {
        FishingLineMesh->ClearAllMeshSections();
        // If there is no spawned bobber actor, show the idle mesh on the rod
        if (IdleBobberMesh) 
        {
            IdleBobberMesh->SetVisibility(CurrentBobber == nullptr);
        }
        return;
    }
    
    // 2. Manage Idle Bobber Visibility
    // If we have a real bobber actor in the world, hide the fake one attached to the mesh
    if (IdleBobberMesh)
    {
        IdleBobberMesh->SetVisibility(CurrentBobber == nullptr);
    }

    // 3. Reset Mesh Buffers
    Vertices.Reset();
    Triangles.Reset();
    Normals.Reset();
    UVs.Reset();
    Tangents.Reset();

    const FTransform ComponentTransform = FishingLineMesh->GetComponentTransform();
    
    // 4. Generate Mesh Geometry
    for (int32 i = 0; i < RopeSolver.Particles.Num() - 1; i++)
    {
        const FVector& StartPoint = RopeSolver.Particles[i].Position;
        const FVector& EndPoint = RopeSolver.Particles[i+1].Position;

        // --- CRITICAL SAFETY CHECK ---
        // If the physics solver exploded, coordinates might be NaN or Infinity.
        // Passing these to the Renderer causes the "CheckMatrixPrecision" crash.
        if (StartPoint.ContainsNaN() || EndPoint.ContainsNaN())
        {
            continue; // Skip this bad segment
        }

        const FVector SegmentDirection = (EndPoint - StartPoint).GetSafeNormal();
        
        // Handle degenerate segments (length is zero)
        if (SegmentDirection.IsNearlyZero()) 
        {
            continue; 
        }

        // Calculate Tube Coordinate Basis
        FVector UpVector = FVector::UpVector;
        // Prevent gimbal lock if looking straight up/down
        if (FMath::Abs(FVector::DotProduct(SegmentDirection, UpVector)) > 0.99f)
        {
            UpVector = FVector::RightVector;
        }
        const FVector RightVector = FVector::CrossProduct(SegmentDirection, UpVector).GetSafeNormal();
        UpVector = FVector::CrossProduct(RightVector, SegmentDirection).GetSafeNormal();

        const int32 RingStartIndex = Vertices.Num();

        // Create Vertices for this Segment Ring
        for (int32 j = 0; j < RopeSides; j++)
        {
            const float Angle = (float)j / (float)RopeSides * 2.0f * PI;
            const FVector Offset = (UpVector * FMath::Sin(Angle) + RightVector * FMath::Cos(Angle)) * RopeWidth;
            const FVector Normal = Offset.GetSafeNormal(); // Normal points out from center

            const FVector WorldPos_Start = StartPoint + Offset;
            const FVector WorldPos_End = EndPoint + Offset;

            // Convert World Space Physics to Local Space Mesh
            Vertices.Add(ComponentTransform.InverseTransformPosition(WorldPos_Start));
            Normals.Add(ComponentTransform.InverseTransformVector(Normal)); 
            UVs.Add(FVector2D((float)i / (RopeSolver.Particles.Num() - 1), (float)j / RopeSides));
            Tangents.Add(FProcMeshTangent(ComponentTransform.InverseTransformVector(RightVector), false));

            Vertices.Add(ComponentTransform.InverseTransformPosition(WorldPos_End));
            Normals.Add(ComponentTransform.InverseTransformVector(Normal));
            UVs.Add(FVector2D((float)(i + 1) / (RopeSolver.Particles.Num() - 1), (float)j / RopeSides));
            Tangents.Add(FProcMeshTangent(ComponentTransform.InverseTransformVector(RightVector), false));
        }

        // Create Triangles
        for (int32 j = 0; j < RopeSides; j++)
        {
            const int32 NextJ = (j + 1) % RopeSides;
            
            // Indices for the quad
            const int32 TopLeft = RingStartIndex + j * 2;
            const int32 BottomLeft = RingStartIndex + j * 2 + 1;
            const int32 TopRight = RingStartIndex + NextJ * 2;
            const int32 BottomRight = RingStartIndex + NextJ * 2 + 1;

            // Triangle 1
            Triangles.Add(TopLeft);
            Triangles.Add(TopRight);
            Triangles.Add(BottomLeft);

            // Triangle 2
            Triangles.Add(BottomLeft);
            Triangles.Add(TopRight);
            Triangles.Add(BottomRight);
        }
    }

    // 5. Commit Mesh to GPU
    if (Triangles.Num() > 0)
    {
        FishingLineMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, false);
        FishingLineMesh->SetMaterial(0, FishingLineMaterial);
    }
    else
    {
        FishingLineMesh->ClearAllMeshSections();
    }

    // 6. Update Idle Bobber Position (Visuals Only)
    if (IdleBobberMesh && IdleBobberMesh->IsVisible() && RopeSolver.Particles.Num() > 0)
    {
        // Snap idle mesh to end of rope
        const FVector LastPos = RopeSolver.Particles.Last().Position;
        
        if (!LastPos.ContainsNaN())
        {
            IdleBobberMesh->SetWorldLocation(LastPos);

            // Rotate to align with the last segment
            if (RopeSolver.Particles.Num() > 1)
            {
                FVector LastSegmentDir = LastPos - RopeSolver.Particles[RopeSolver.Particles.Num() - 2].Position;
                if (!LastSegmentDir.IsNearlyZero())
                {
                    IdleBobberMesh->SetWorldRotation(LastSegmentDir.Rotation());
                }
            }
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
    UE_LOG(LogSolaraqPhysics, Warning, TEXT(">>> CAST REQUEST STARTED <<<"));

    // 1. Calculate Max Length (Target) but DO NOT spawn particles there yet
    TargetRopeLength = FMath::Lerp(MinCastRopeLength, MaxRopeLength, Charge);
    
    // 2. Setup Launch Vector
    const FVector RodTipLoc = RodMesh->GetSocketLocation(RodTipSocketName);
    FVector CrossAxis = FVector::CrossProduct(HorizontalCastDirection, FVector::UpVector);
    if (CrossAxis.IsNearlyZero()) CrossAxis = FVector::RightVector;
    const FVector LaunchDir = HorizontalCastDirection.RotateAngleAxis(CastAngle, CrossAxis.GetSafeNormal()).GetSafeNormal();

    // 3. Reset Solver to minimal state (RodTip -> Bobber, very short)
    TArray<FVector> Points;
    Points.Add(RodTipLoc);
    Points.Add(RodTipLoc + (LaunchDir * 10.0f)); // Small offset

    RopeSolver.Initialize(Points, RopeMass); // Initialize with just 2 points
    CurrentRopeLength = 10.0f; 

    // 4. Spawn Bobber AT ROD TIP
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = GetInstigator();
    
    CurrentBobber = GetWorld()->SpawnActor<AFishingBobber>(BobberClass, Points.Last(), LaunchDir.Rotation(), SpawnParams);
    
    if (CurrentBobber)
    {
        float CastSpeed = FMath::Lerp(400.f, 1380.f, Charge);
        CurrentBobber->ProjectileMovement->Velocity = LaunchDir * CastSpeed;
         
        UE_LOG(LogSolaraqPhysics, Log, TEXT("Bobber Launched from %s. Velocity: %f"), *RodTipLoc.ToString(), CastSpeed);
    }

    bIsCasting = true; 
    bIsReeling = false;
    bBobberHasLanded = false;
    IdleBobberMesh->SetVisibility(false);

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
    UE_LOG(LogSolaraqFishing, Log, TEXT("Rod (%s): NotifyFishBite() triggered."), *GetName());

    // 1. Play Sound Effect
    if (FishBiteSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, FishBiteSound, GetActorLocation());
    }

    // 2. Apply Physical Force / Visual Dip
    if (CurrentBobber)
    {
        // CASE A: We have a Bobber.
        // In Tick(), the Rope is pinned to the Bobber's location. 
        // To make the line jerk, we jerk the Bobber downwards.
        
        FVector BobberLoc = CurrentBobber->GetActorLocation();
        
        // Instantly dip the bobber down (e.g., 25 units).
        // Since the bobber physics are likely disabled/kinematic while floating, 
        // SetActorLocation is the most reliable way to force this.
        BobberLoc.Z -= 25.0f; 
        
        CurrentBobber->SetActorLocation(BobberLoc);

        // The XPBD Solver in Tick() will perceive this sudden change in position
        // as a high-velocity event and the constraint forces will pull the 
        // rest of the line down automatically.
    }
    else if (RopeSolver.Particles.Num() > 0)
    {
        // CASE B: Bare line (no bobber).
        // We manipulate the last particle of the solver directly.
        
        FXPBDParticle& LastParticle = RopeSolver.Particles.Last();
        
        if (LastParticle.InverseMass > 0.0f)
        {
            // If the particle is dynamic, add a sharp downward velocity impulse.
            // (Simulates a pull force)
            LastParticle.Velocity += FVector(0.0f, 0.0f, -800.0f);
        }
        else
        {
            // If the particle is pinned (Kinematic), we must manually offset 
            // the position to visualize the pull, effectively overriding the pin 
            // for this instant.
            LastParticle.Position.Z -= 25.0f;
            
            // Note: We do NOT update PrevPosition here. By moving Position
            // but leaving PrevPosition alone, we implicitly create a massive
            // velocity for the solver to resolve in the next step.
        }
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
    if (bBobberHasLanded) return;
    
    UE_LOG(LogSolaraqPhysics, Log, TEXT("Bobber Landed. Locking unspooling."));
    bBobberHasLanded = true;
    bIsCasting = false; // Stop adding new particles
    
    // Lock the Target Length to whatever we actually achieved
    TargetRopeLength = RopeSolver.GetTotalRestLength();
    CurrentRopeLength = TargetRopeLength;
}
