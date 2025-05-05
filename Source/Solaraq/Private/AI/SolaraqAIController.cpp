// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SolaraqAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Pawns/SolaraqShipBase.h" // Include your ship base class
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"
#include "Logging/SolaraqLogChannels.h" // Your custom log channel
#include "Kismet/GameplayStatics.h" // For getting player pawn potentially
#include "AI/SolaraqAIController.h"
#include "Pawns/SolaraqEnemyShip.h"
#include "Components/SphereComponent.h"


bool ASolaraqAIController::CalculateInterceptPoint(
    FVector ShooterLocation, FVector ShooterVelocity,
    FVector TargetLocation, FVector TargetVelocity,
    float ProjectileSpeed, FVector& InterceptPoint)
{
    const FVector RelativePosition = TargetLocation - ShooterLocation;
    const FVector RelativeVelocity = TargetVelocity - ShooterVelocity;

    const float a = FVector::DotProduct(RelativeVelocity, RelativeVelocity) - FMath::Square(ProjectileSpeed);
    const float b = 2.0f * FVector::DotProduct(RelativePosition, RelativeVelocity);
    const float c = FVector::DotProduct(RelativePosition, RelativePosition);

    float t = -1.0f; // Intercept time, initialize to invalid

    if (FMath::IsNearlyZero(a)) // Check for linear equation case (relative speed ~= projectile speed)
    {
        if (!FMath::IsNearlyZero(b))
        {
            t = -c / b;
        }
        // else: Both a and b are zero, c is likely also zero (already colliding?) or non-zero (parallel, never catch up). No solution.
    }
    else
    {
        const float Discriminant = FMath::Square(b) - 4.0f * a * c;

        if (Discriminant >= 0.0f) // Check for real solutions
        {
            const float SqrtDiscriminant = FMath::Sqrt(Discriminant);
            const float t1 = (-b + SqrtDiscriminant) / (2.0f * a);
            const float t2 = (-b - SqrtDiscriminant) / (2.0f * a);

            // Select the smallest positive time
            if (t1 > KINDA_SMALL_NUMBER && (t2 <= KINDA_SMALL_NUMBER || t1 < t2))
            {
                t = t1;
            }
            else if (t2 > KINDA_SMALL_NUMBER)
            {
                t = t2;
            }
            // Else: Both solutions are non-positive, no future intercept.
        }
        // Else: Discriminant < 0, no real solutions (target too fast/far).
    }

    if (t > KINDA_SMALL_NUMBER) // We found a valid positive time
    {
        InterceptPoint = TargetLocation + TargetVelocity * t;
         // Optional Logging:
        // UE_LOG(LogSolaraqAI, Verbose, TEXT("Intercept Calculated: t=%.3f, Point=%s"), t, *InterceptPoint.ToString());
        return true;
    }
    else // No valid positive time found
    {
        // Fallback: Aim directly at the target if prediction fails?
        InterceptPoint = TargetLocation;
        // UE_LOG(LogSolaraqAI, Verbose, TEXT("Intercept Failed. Aiming directly. a=%.2f, b=%.2f, c=%.2f"), a, b, c);
        return false;
    }
}

ASolaraqAIController::ASolaraqAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Set Tick Interval
    // Tick only 10 times per second (every 0.2 seconds)
    PrimaryActorTick.TickInterval = 0.1f;
    
    // --- Create Components ---
    PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));

    // --- Configure Sight Sense ---
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    if (SightConfig)
    {
        SightConfig->SightRadius = 15000.0f; // How far can it see?
        SightConfig->LoseSightRadius = 18000.0f; // How far until it forgets? (Needs to be > SightRadius)
        SightConfig->PeripheralVisionAngleDegrees = 180.0f; // Field of view (180 for full forward)
        SightConfig->SetMaxAge(5.0f); // How long stimuli memory lasts (seconds)

        // Detect enemies (assuming player is enemy - need affiliation setup later)
        SightConfig->DetectionByAffiliation.bDetectEnemies = true;
        SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
        SightConfig->DetectionByAffiliation.bDetectNeutrals = false;

        // --- Assign Sight Config to Perception Component ---
        PerceptionComponent->ConfigureSense(*SightConfig);
        PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
    }
    else
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("Failed to create SightConfig for %s"), *GetName());
    }

    // Don't need a Blackboard component

    // Default state
    CurrentTargetActor = nullptr;
    bHasLineOfSight = false;

    UE_LOG(LogSolaraqAI, Warning, TEXT("ASolaraqAIController %s Constructed"), *GetName());
}

FGenericTeamId ASolaraqAIController::GetGenericTeamId() const
{
    return TeamId;
}

ETeamAttitude::Type ASolaraqAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
    if (const APawn* OtherPawn = Cast<const APawn>(&Other))
    {
        // --- Check 1: Try getting team from the Other's CONTROLLER first ---
        if (const IGenericTeamAgentInterface* TeamAgentController = Cast<const IGenericTeamAgentInterface>(OtherPawn->GetController()))
        {
            FGenericTeamId OtherTeamId = TeamAgentController->GetGenericTeamId();
            if (OtherTeamId == TeamId) { return ETeamAttitude::Friendly; }
            if (OtherTeamId.GetId() != FGenericTeamId::NoTeam.GetId()) { return ETeamAttitude::Hostile; }
            // If Controller is NoTeam or doesn't implement, fall through...
        }

        // --- Check 2: Try getting team from the Other PAWN itself ---
        if (const IGenericTeamAgentInterface* TeamAgentPawn = Cast<const IGenericTeamAgentInterface>(OtherPawn))
        {
            FGenericTeamId OtherTeamId = TeamAgentPawn->GetGenericTeamId();
            if (OtherTeamId == TeamId) { return ETeamAttitude::Friendly; }
            if (OtherTeamId.GetId() != FGenericTeamId::NoTeam.GetId()) { return ETeamAttitude::Hostile; }
             // If Pawn is NoTeam or doesn't implement, fall through...
        }

        // --- Fallback: If specifically controlled by a PlayerController (and checks above failed) ---
        // This helps if the player pawn *doesn't* have the interface but the controller is identifiable.
        // NOTE: Be careful, this assumes ALL players are hostile if not otherwise specified.
        if (Cast<APlayerController>(OtherPawn->GetController()))
        {
             return ETeamAttitude::Hostile;
        }
    }
    // --- Check 3: Check non-Pawns directly (e.g., turrets?) ---
    else if (const IGenericTeamAgentInterface* TeamAgent = Cast<const IGenericTeamAgentInterface>(&Other))
    {
         FGenericTeamId OtherTeamId = TeamAgent->GetGenericTeamId();
         if (OtherTeamId == TeamId) { return ETeamAttitude::Friendly; }
         if (OtherTeamId.GetId() != FGenericTeamId::NoTeam.GetId()) { return ETeamAttitude::Hostile; }
    }


    // Default attitude towards everything else
    return ETeamAttitude::Neutral;
}

void ASolaraqAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    ControlledEnemyShip = Cast<ASolaraqEnemyShip>(InPawn); // Store in a ASolaraqEnemyShip* variable
    if (!ControlledEnemyShip)
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("ASolaraqAIController possessed a non-SolaraqShipBase Pawn: %s"), *GetNameSafe(InPawn));
        return;
    }

    UE_LOG(LogSolaraqAI, Warning, TEXT("ASolaraqAIController possessed %s"), *ControlledEnemyShip->GetName());

    // --- Bind Perception Delegate ---
    if (PerceptionComponent)
    {
        UE_LOG(LogSolaraqSystem, Log, TEXT("Binding OnPerceptionUpdated..."));
        PerceptionComponent->OnPerceptionUpdated.AddDynamic(this, &ASolaraqAIController::HandlePerceptionUpdated);
        // Alternatively, bind to OnTargetPerceptionUpdated for more detailed stimulus info:
        // PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ASolaraqAIController::HandleTargetPerceptionUpdated);
    }
    else
    {
        UE_LOG(LogSolaraqSystem, Error, TEXT("PerceptionComponent is null on %s during OnPossess!"), *GetName());
    }
}

void ASolaraqAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

     // --- Initial Check & State Log ---
    if (!ControlledEnemyShip) // Check pointer validity first
    {
        // Only log if it's unexpected, maybe happens during cleanup
        UE_LOG(LogSolaraqAI, Warning, TEXT("Tick: No ControlledEnemyShip"));
        return;
    }
    if (ControlledEnemyShip->IsDead()) // Check if pawn is dead
    {
        if (CurrentTargetActor.IsValid() || bHasLineOfSight) // Log only if clearing state due to death
        {
            UE_LOG(LogSolaraqAI, Warning, TEXT("%s Tick: Controlled Ship is Dead. Clearing target."), *GetName());
            CurrentTargetActor = nullptr;
            bHasLineOfSight = false;
        }
        return;
    }

    // --- High-Visibility Log: State at Start of Tick ---
    UE_LOG(LogSolaraqAI, Warning, TEXT("%s TICK CHECK ----> Target: [%s], HasLoS: [%d]"),
           *GetName(),
           *GetNameSafe(CurrentTargetActor.Get()), // Use GetNameSafe for TWeakObjectPtr
           bHasLineOfSight);

    // --- Core AI Logic Loop ---

    if (CurrentTargetActor.IsValid() && bHasLineOfSight)
    {
        UE_LOG(LogSolaraqAI, Warning, TEXT("%s Tick State: === ATTACKING [%s] ==="), *GetName(), *CurrentTargetActor->GetName()); // <<< STATE LOG

        // --- ATTACKING STATE ---
        AActor* Target = CurrentTargetActor.Get(); // Get valid pointer for this scope
        if (!Target) {
             UE_LOG(LogSolaraqAI, Warning, TEXT("%s Tick: Target became invalid during ATTACKING state!"), *GetName());
             // Force re-evaluation next tick by clearing state here?
             CurrentTargetActor = nullptr;
             bHasLineOfSight = false;
             return;
        }


        // 1. Calculate Aim Point
        FVector ShooterLoc = ControlledEnemyShip->GetActorLocation();
        FVector ShooterVel = ControlledEnemyShip->GetCollisionAndPhysicsRoot()->GetPhysicsLinearVelocity();
        FVector TargetLoc = Target->GetActorLocation(); // Use validated Target pointer
        APawn* TargetPawn = Cast<APawn>(Target);
        FVector TargetVel = TargetPawn ? TargetPawn->GetVelocity() : FVector::ZeroVector;
        float ProjectileSpeed = 5000.0f; // TODO: Get this from weapon

        bool bPredictionSuccess = CalculateInterceptPoint(ShooterLoc, ShooterVel, TargetLoc, TargetVel, ProjectileSpeed, PredictedAimLocation);

        if (bPredictionSuccess)
        {
             UE_LOG(LogSolaraqAI, Warning, TEXT("%s Attack: Prediction SUCCESS -> Aiming at %s"), *GetName(), *PredictedAimLocation.ToString());
        }
        else
        {
             UE_LOG(LogSolaraqAI, Warning, TEXT("%s Attack: Prediction FAILED -> Aiming directly at %s"), *GetName(), *PredictedAimLocation.ToString());
        }

        // 2. Turn Towards Aim Point
        UE_LOG(LogSolaraqAI, Warning, TEXT("%s Attack: Calling TurnTowards(%s)"), *GetName(), *PredictedAimLocation.ToString());
        ControlledEnemyShip->TurnTowards(PredictedAimLocation); // Make sure logs inside this function exist too!

        // 3. Fire Weapon (If aiming correctly)
        FVector DirectionToTarget = (PredictedAimLocation - ControlledEnemyShip->GetActorLocation()).GetSafeNormal();
        FVector ForwardVec = ControlledEnemyShip->GetActorForwardVector();
        float AimDotProduct = FVector::DotProduct(ForwardVec, DirectionToTarget);

        UE_LOG(LogSolaraqAI, Warning, TEXT("%s Attack: Aim Dot Product: %.3f (Threshold > 0.98)"), *GetName(), AimDotProduct);

        if (AimDotProduct > 0.98f) // Check if facing target
        {
            UE_LOG(LogSolaraqAI, Log, TEXT("%s Attack: Aim condition MET. Calling FireWeapon..."), *GetName());
            ControlledEnemyShip->FireWeapon(); // Make sure logs inside this function exist too!
        }
         else
         {
              UE_LOG(LogSolaraqAI, Warning, TEXT("%s Attack: Aim condition NOT MET."), *GetName());
         }

        // 4. Adjust Position? (Optional)
        // ... (Add logging here if you implement movement) ...

    }
    else if (CurrentTargetActor.IsValid() && !bHasLineOfSight)
    {
         UE_LOG(LogSolaraqAI, Warning, TEXT("%s Tick State: === SEARCHING for [%s] ==="), *GetName(), *CurrentTargetActor->GetName()); // <<< STATE LOG
        // --- SEARCHING STATE (Target lost) ---
        UE_LOG(LogSolaraqAI, Warning, TEXT("%s Search: Calling TurnTowards last known: %s"), *GetName(), *LastKnownTargetLocation.ToString());
        ControlledEnemyShip->TurnTowards(LastKnownTargetLocation);
        ControlledEnemyShip->RequestMoveForward(0.0f); // Stop moving
    }
    else
    {
         UE_LOG(LogSolaraqAI, Warning, TEXT("%s Tick State: === IDLE ==="), *GetName()); // <<< STATE LOG
        // --- IDLE STATE ---
        ControlledEnemyShip->RequestMoveForward(0.0f); // Ensure stopped
    }
}

void ASolaraqAIController::HandlePerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
    UE_LOG(LogSolaraqAI, Warning, TEXT("%s Perception Updated. %d actors reported."), *GetName(), UpdatedActors.Num());

    TArray<AActor*> PerceivedActors;
    PerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

    UE_LOG(LogSolaraqAI, Warning, TEXT("Currently Perceived Actors (Sight): %d"), PerceivedActors.Num());

    UpdateTargetActor(PerceivedActors);
}

// Example implementation using OnTargetPerceptionUpdated instead:
/*
void ASolaraqAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    if (!Actor) return;

    UE_LOG(LogSolaraqAI, Verbose, TEXT("%s Target Perception Updated for Actor %s. Sensed: %d"),
        *GetName(), *Actor->GetName(), Stimulus.WasSuccessfullySensed());

    ASolaraqShipBase* TargetShip = Cast<ASolaraqShipBase>(Actor); // Or Player Pawn class
    if (!TargetShip) return; // Ignore non-ships for now

    if (Stimulus.WasSuccessfullySensed())
    {
        // Target Acquired or Updated
        CurrentTargetActor = TargetShip;
        LastKnownTargetLocation = Stimulus.StimulusLocation; // More accurate than ActorLocation sometimes
        PredictedAimLocation = LastKnownTargetLocation; // Default aim before prediction
        bHasLineOfSight = true;
        UE_LOG(LogSolaraqAI, Log, TEXT("Target %s acquired/updated."), *Actor->GetName());
    }
    else
    {
        // Target Lost
        if (CurrentTargetActor == Actor) // Check if it's our *current* target that was lost
        {
            UE_LOG(LogSolaraqAI, Log, TEXT("Target %s lost."), *Actor->GetName());
            // Keep CurrentTargetActor reference but mark LoS as false
            LastKnownTargetLocation = Stimulus.StimulusLocation; // Last known place
            bHasLineOfSight = false;
            // Optionally: Clear CurrentTargetActor completely after a timeout?
            // CurrentTargetActor = nullptr;
        }
    }
}
*/

void ASolaraqAIController::UpdateTargetActor(const TArray<AActor*>& PerceivedActors)
{
    AActor* BestTarget = nullptr;
    float BestTargetDistSq = FLT_MAX; // Use FLT_MAX from limits.h or float limits

    UE_LOG(LogSolaraqAI, Warning, TEXT("UpdateTargetActor: Checking %d perceived actors."), PerceivedActors.Num()); // <<< ADDED LOG

    for (AActor* PerceivedActor : PerceivedActors)
    {
        if (!PerceivedActor) continue;

        UE_LOG(LogSolaraqAI, Warning, TEXT("  -> Checking actor: %s"), *PerceivedActor->GetName()); // <<< ADDED LOG

        // Check Affiliation/Tags
        ETeamAttitude::Type Attitude = GetTeamAttitudeTowards(*PerceivedActor); // <<< CHECK ATTITUDE
        UE_LOG(LogSolaraqAI, Warning, TEXT("  -> Attitude towards %s: %d"), *PerceivedActor->GetName(), static_cast<int32>(Attitude)); // <<< LOG ATTITUDE

        if (Attitude == ETeamAttitude::Hostile) // <<< ENSURE HOSTILE CHECK
        {
            ASolaraqShipBase* PerceivedShip = Cast<ASolaraqShipBase>(PerceivedActor);
            if (PerceivedShip && PerceivedShip != ControlledEnemyShip && !PerceivedShip->IsDead())
            {
                 UE_LOG(LogSolaraqAI, Warning, TEXT("  -> Actor %s is a valid Hostile Ship."), *PerceivedActor->GetName()); // <<< ADDED LOG
                float DistSq = FVector::DistSquared(ControlledEnemyShip->GetActorLocation(), PerceivedShip->GetActorLocation());
                if (DistSq < BestTargetDistSq)
                {
                    BestTargetDistSq = DistSq;
                    BestTarget = PerceivedActor;
                    UE_LOG(LogSolaraqAI, Warning, TEXT("  -> %s is NEW closest valid target."), *BestTarget->GetName()); // <<< ADDED LOG
                }
            }
            else
            {
                 UE_LOG(LogSolaraqAI, Warning, TEXT("  -> Actor %s is Hostile BUT failed ship check (IsShip=%d, IsSelf=%d, IsDead=%d)"),
                      *PerceivedActor->GetName(), IsValid(PerceivedShip), PerceivedShip == ControlledEnemyShip, IsValid(PerceivedShip) ? PerceivedShip->IsDead() : -1 ); // <<< ADDED LOG
            }
        }
         else { UE_LOG(LogSolaraqAI, Warning, TEXT("  -> Actor %s is not Hostile."), *PerceivedActor->GetName()); } // <<< ADDED LOG
    }

    // --- Update State based on BestTarget found ---
    if (BestTarget)
    {
        if (CurrentTargetActor != BestTarget)
        {
            UE_LOG(LogSolaraqAI, Warning, TEXT("%s ACQUIRED new target: %s"), *GetName(), *BestTarget->GetName()); // <<< LOG TARGET ACQUISITION
            CurrentTargetActor = BestTarget;
            PredictedAimLocation = BestTarget->GetActorLocation();
        }
        LastKnownTargetLocation = BestTarget->GetActorLocation();
        bHasLineOfSight = true;
         UE_LOG(LogSolaraqAI, Warning, TEXT("%s Target set to %s, HasLoS=true"), *GetName(), *BestTarget->GetName()); // <<< LOG FINAL STATE
    }
    else // No valid target currently perceived
    {
         UE_LOG(LogSolaraqAI, Warning, TEXT("%s No valid best target found this update."), *GetName()); // <<< LOG NO TARGET
        if (CurrentTargetActor.IsValid())
        {
             UE_LOG(LogSolaraqAI, Warning, TEXT("%s LOST sight of target %s"), *GetName(), *CurrentTargetActor->GetName()); // <<< LOG TARGET LOSS
             // LastKnownTargetLocation already set
        }
        bHasLineOfSight = false;
        // Maybe clear target completely if LoS lost? Depends on desired behavior
        // CurrentTargetActor = nullptr;
    }
}