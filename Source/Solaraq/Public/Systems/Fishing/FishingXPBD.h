// Systems/Fishing/FishingXPBD.h
#pragma once

#include "CoreMinimal.h"
#include "DrawDebugHelpers.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSolaraqPhysics, Log, All);

struct FXPBDParticle
{
    FVector Position;
    FVector PrevPosition;
    FVector Velocity;
    float InverseMass;
    int32 Index;
};

struct FXPBDConstraint
{
    int32 IndexA;
    int32 IndexB;
    float RestLength;
    float Compliance = 0.0f;
    float Lambda = 0.0f;
};

class FXPBDSolver
{
public:
    TArray<FXPBDParticle> Particles;
    TArray<FXPBDConstraint> Constraints;

    // Reduced substeps slightly for stability/performance balance
    int32 SubSteps = 5; 
    FVector Gravity = FVector(0, 0, -980.0f);
    float BaseCompliance = 0.0f; 

    // --- Public API ---

    void Initialize(const TArray<FVector>& InitialPositions, float ParticleMass)
    {
        // (Same as before)
        Particles.Empty();
        Constraints.Empty();

        float InvMass = (ParticleMass > 0.0f) ? 1.0f / ParticleMass : 0.0f;

        for (int32 i = 0; i < InitialPositions.Num(); i++)
        {
            FXPBDParticle P;
            P.Position = InitialPositions[i];
            P.PrevPosition = InitialPositions[i];
            P.Velocity = FVector::ZeroVector;
            P.InverseMass = InvMass;
            P.Index = i;
            Particles.Add(P);
        }

        RebuildConstraints(BaseCompliance);
    }

    void Simulate(float dt, UWorld* World)
    {
        if (dt <= 0.0f || Particles.Num() < 2) return;
        
        // Clamp dt to prevent explosion on lag spikes
        float SafeDt = FMath::Min(dt, 0.1f); 

        float SubStepDt = SafeDt / (float)SubSteps;
        float Alpha = BaseCompliance / (SubStepDt * SubStepDt);

        for (int32 Step = 0; Step < SubSteps; ++Step)
        {
            // 1. Predict
            for (FXPBDParticle& P : Particles)
            {
                if (P.InverseMass == 0.0f) continue;

                P.PrevPosition = P.Position;
                P.Velocity += Gravity * SubStepDt;
                P.Position += P.Velocity * SubStepDt;
            }

            // 2. Solve
            SolveConstraints(SubStepDt, Alpha);

            // 3. Ground Collision (Pass SubStepDt now!)
            SolveGroundCollisions(World, SubStepDt);

            // 4. Update Velocity
            for (FXPBDParticle& P : Particles)
            {
                if (P.InverseMass == 0.0f) continue;
                
                P.Velocity = (P.Position - P.PrevPosition) / SubStepDt;
                
                // Damping
                P.Velocity *= 0.99f; 
                
                // SAFETY: NaN Check
                if (P.Velocity.ContainsNaN()) P.Velocity = FVector::ZeroVector;
            }
        }
    }

    // ... (Insert/Remove Helper functions same as before) ...
    void InsertParticleAfterHead(const FVector& Position, float Mass, float Compliance)
    {
        if (Particles.Num() < 1) return;
        FXPBDParticle NewP;
        NewP.Position = Position;
        NewP.PrevPosition = Position;
        NewP.Velocity = FVector::ZeroVector;
        NewP.InverseMass = (Mass > 0.0f) ? 1.0f / Mass : 0.0f;
        Particles.Insert(NewP, 1);
        RebuildConstraints(Compliance);
    }

    void RemoveParticleAfterHead(float Compliance)
    {
        if (Particles.Num() <= 2) return;
        Particles.RemoveAt(1);
        RebuildConstraints(Compliance);
    }

    void RebuildConstraints(float Compliance)
    {
        Constraints.Empty();
        for (int32 i = 0; i < Particles.Num() - 1; i++)
        {
            FXPBDConstraint C;
            C.IndexA = i;
            C.IndexB = i + 1;
            C.RestLength = FVector::Dist(Particles[i].Position, Particles[i + 1].Position);
            C.Compliance = Compliance;
            C.Lambda = 0.0f;
            Constraints.Add(C);
        }
    }
    
    float GetTotalRestLength() const
    {
        float Total = 0.0f;
        for (const FXPBDConstraint& C : Constraints) Total += C.RestLength;
        return Total;
    }

    void DebugDraw(UWorld* World)
    {
        if (!World) return;
        for (const FXPBDConstraint& C : Constraints)
        {
            if (Particles.IsValidIndex(C.IndexA) && Particles.IsValidIndex(C.IndexB))
            {
                 // Add Safety Check here too
                 FVector P1 = Particles[C.IndexA].Position;
                 FVector P2 = Particles[C.IndexB].Position;
                 if(!P1.ContainsNaN() && !P2.ContainsNaN())
                 {
                    DrawDebugLine(World, P1, P2, FColor::Green, false, -1.0f, 0, 2.0f);
                 }
            }
        }
    }

private:
    void SolveConstraints(float dt, float Alpha)
    {
        for (FXPBDConstraint& C : Constraints)
        {
            FXPBDParticle& P1 = Particles[C.IndexA];
            FXPBDParticle& P2 = Particles[C.IndexB];

            float w1 = P1.InverseMass;
            float w2 = P2.InverseMass;
            float wSum = w1 + w2;

            if (wSum == 0.0f) continue;

            FVector Delta = P1.Position - P2.Position;
            float Dist = Delta.Size();
            
            // SAFETY: Prevent division by zero if particles overlap perfectly
            if (Dist < KINDA_SMALL_NUMBER) 
            {
                Dist = KINDA_SMALL_NUMBER;
                Delta = FVector(0,0,KINDA_SMALL_NUMBER); 
            }

            FVector n = Delta / Dist;
            float ConstraintError = Dist - C.RestLength; 
            float DeltaLambda = (-ConstraintError - Alpha * C.Lambda) / (wSum + Alpha);

            C.Lambda += DeltaLambda;
            FVector Correction = n * DeltaLambda;

            if (w1 > 0.0f) P1.Position += Correction * w1;
            if (w2 > 0.0f) P2.Position -= Correction * w2;
        }
    }
    
    void SolveGroundCollisions(UWorld* World, float dt)
    {
        if (!World) return;

        for (FXPBDParticle& P : Particles)
        {
            // Skip pinned particles (InvMass = 0)
            if (P.InverseMass == 0.0f) continue;

            // SAFETY: Check for NaNs before we start
            if (P.Position.ContainsNaN() || P.PrevPosition.ContainsNaN())
            {
                P.Position = FVector::ZeroVector;
                P.PrevPosition = FVector::ZeroVector;
                P.Velocity = FVector::ZeroVector;
                continue;
            }

            FHitResult Hit;
            FVector Start = P.PrevPosition;
            
            // Determine where we check for collision
            FVector TraceEnd;
            FVector MovementDelta = P.Position - P.PrevPosition;
            float MovementDist = MovementDelta.Size();

            // LOGIC: If moving fast, look ahead. If barely moving, look down to stay grounded.
            if (MovementDist < 0.5f) 
            {
                // "Resting" check: trace down slightly to handle resting on uneven ground
                // without this, it might fall through the floor if velocity is near zero
                TraceEnd = P.Position - FVector(0, 0, 5.0f); 
            }
            else 
            {
                // Moving: trace slightly past the destination to catch collision
                TraceEnd = P.Position + (MovementDelta.GetSafeNormal() * 2.0f);
            }

            // Perform Trace
            bool bHit = World->LineTraceSingleByChannel(Hit, Start, TraceEnd, ECC_WorldStatic);

            if (bHit)
            {
                // 1. Project out of collision
                // FIX: Reduced push-out from 2.0f to 0.2f.
                // Large values here cause the "pop-up" jitter (vibrating up and down).
                P.Position = Hit.ImpactPoint + (Hit.ImpactNormal * 0.2f);

                // 2. Friction Calculation
                // Remove velocity going INTO the wall (Normal Component)
                FVector NormalComp = Hit.ImpactNormal * FVector::DotProduct(P.Velocity, Hit.ImpactNormal);
                // Isolate velocity sliding ALONG the wall (Tangent Component)
                FVector TangentComp = P.Velocity - NormalComp;
                
                // FIX: Stronger Friction. 
                // Multiplier 0.1f means it loses 90% of its slide speed every frame on contact.
                // This stops the "ghost sliding" effect.
                P.Velocity = TangentComp * 0.1f; 

                // 3. Update PrevPosition
                // In XPBD, constraints are solved based on P - PrevP. 
                // Since we manually modified P and Velocity, we must back-calculate PrevP
                // so the solver respects our friction/collision in the next step.
                P.PrevPosition = P.Position - (P.Velocity * dt); 
            }
        }
    }
};