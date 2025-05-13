// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SolaraqAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Utils/SolaraqMathLibrary.h"
#include "Pawns/SolaraqShipBase.h" // Include your ship base class
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"
#include "Logging/SolaraqLogChannels.h" // Your custom log channel
#include "Kismet/GameplayStatics.h" // For getting player pawn potentially
#include "AI/SolaraqAIController.h"
#include "Pawns/SolaraqEnemyShip.h"
#include "Components/SphereComponent.h"


ASolaraqAIController::ASolaraqAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Set Tick Interval
    // Tick only 10 times per second (every 0.2 seconds)
    PrimaryActorTick.TickInterval = 0.05f;
    
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
        //UE_LOG(LogSolaraqAI, Warning, TEXT("Failed to create SightConfig for %s"), *GetName());
    }

    // Don't need a Blackboard component

    // Default state
    CurrentTargetActor = nullptr;
    bHasLineOfSight = false;

    //UE_LOG(LogSolaraqAI, Warning, TEXT("ASolaraqAIController %s Constructed"), *GetName());
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
        //UE_LOG(LogSolaraqAI, Warning, TEXT("ASolaraqAIController possessed a non-SolaraqShipBase Pawn: %s"), *GetNameSafe(InPawn));
        return;
    }

    //UE_LOG(LogSolaraqAI, Warning, TEXT("ASolaraqAIController possessed %s"), *ControlledEnemyShip->GetName());

    // --- Bind Perception Delegate ---
    if (PerceptionComponent)
    {
        //UE_LOG(LogSolaraqSystem, Log, TEXT("Binding OnPerceptionUpdated..."));
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
    if (!ControlledEnemyShip || ControlledEnemyShip->IsDead())
    {
        // Clear state if pawn died or is invalid
        if (CurrentTargetActor.IsValid() || bHasLineOfSight || bIsPerformingBoostTurn)
        {
            UE_LOG(LogSolaraqAI, Warning, TEXT("%s Tick: Controlled Ship Invalid/Dead. Clearing state."), *GetName());
            CurrentTargetActor = nullptr;
            bHasLineOfSight = false;
            bIsPerformingBoostTurn = false;
            if (ControlledEnemyShip && ControlledEnemyShip->IsBoosting())
            {
                ControlledEnemyShip->Server_SetAttemptingBoost(false); // Ensure boost stops
            }
        }
        ExecuteIdleMovement(); // Ensure ship stops moving
        return;
    }

    // --- Get Current State Info ---
    AActor* Target = CurrentTargetActor.Get(); // Get valid pointer if weak ptr is valid
    const FVector ShipLocation = ControlledEnemyShip->GetActorLocation();
    const FVector ShipForward = ControlledEnemyShip->GetActorForwardVector();
    const FVector ShipVelocity = ControlledEnemyShip->GetCollisionAndPhysicsRoot() ? ControlledEnemyShip->GetCollisionAndPhysicsRoot()->GetPhysicsLinearVelocity() : FVector::ZeroVector;

    // --- High-Visibility Log: State at Start of Tick ---
    UE_LOG(LogSolaraqAI, Log, TEXT("%s TICK CHECK ----> Target: [%s], HasLoS: [%d], BoostTurning: [%d]"),
           *GetName(),
           *GetNameSafe(CurrentTargetActor.Get()), // Use GetNameSafe for TWeakObjectPtr
           bHasLineOfSight,
           bIsPerformingBoostTurn);


    // --- Core AI Logic Loop ---

    if (Target && bHasLineOfSight) // Target is valid and we can see it
    {
        // ******** ADDED CALCULATIONS HERE ********
        const FVector TargetLocation = Target->GetActorLocation();
        const APawn* TargetPawn = Cast<APawn>(Target);
        const FVector TargetVelocity = TargetPawn ? TargetPawn->GetVelocity() : FVector::ZeroVector; // Needed for prediction
        const float DistanceToTarget = FVector::Dist(ShipLocation, TargetLocation);
        const float AngleToTarget = GetAngleToTarget(TargetLocation); // Calculate angle to target
        // ****************************************

        // --- Determine Movement Behavior & Execute ---
        // Note: DogfightRange is a member variable (UPROPERTY) and should be accessible here
        if (!bIsPerformingBoostTurn && AngleToTarget > ReversalAngleThreshold)
        {
            UE_LOG(LogSolaraqAI, Error, TEXT("%s Tick State: === STARTING BOOST TURN (Angle: %.1f > %.1f) ==="), *GetName(), AngleToTarget, ReversalAngleThreshold);
            bIsPerformingBoostTurn = true;
            ExecuteReversalTurnMovement(TargetLocation, AngleToTarget, DeltaTime); // Pass calculated values
            CurrentDogfightState = EDogfightState::OffsetApproach; // Reset dogfight state if boosting
            TimeInCurrentDogfightState = 0.0f;
        }
        else if (bIsPerformingBoostTurn)
        {
             UE_LOG(LogSolaraqAI, Error, TEXT("%s Tick State: === CONTINUING BOOST TURN (Angle: %.1f) ==="), *GetName(), AngleToTarget);
            ExecuteReversalTurnMovement(TargetLocation, AngleToTarget, DeltaTime); // Pass calculated values
        }
        else if (DistanceToTarget <= DogfightRange) // Check Dogfight Range (Uses member variable)
        {
            // Dogfight state is managed internally by ExecuteDogfightMovement now
             ExecuteDogfightMovement(Target, DeltaTime); // Pass the Target actor
        }
        else // Default to Chasing
        {
             UE_LOG(LogSolaraqAI, Error, TEXT("%s Tick State: === CHASING [%s] (Dist: %.0f > %.0f) ==="), *GetName(), *Target->GetName(), DistanceToTarget, DogfightRange);
             ExecuteChaseMovement(TargetLocation, DeltaTime); // Pass calculated value
             CurrentDogfightState = EDogfightState::OffsetApproach; // Reset dogfight state if chasing
             TimeInCurrentDogfightState = 0.0f;
        }

        // --- Aiming & Firing (Common Logic) ---
         if (!bIsPerformingBoostTurn)
         {
             // Prediction needs TargetLocation, TargetVelocity, ShipLocation, ShipVelocity
             float ProjectileSpeed = ControlledEnemyShip->GetProjectileMuzzleSpeed();
             bool bPredictionSuccess = USolaraqMathLibrary::CalculateInterceptPoint(ShipLocation, ShipVelocity, TargetLocation, TargetVelocity, ProjectileSpeed, PredictedAimLocation);
             if (!bPredictionSuccess) PredictedAimLocation = TargetLocation;

             // Aiming/Firing conditional logic
             bool bShouldAimAndFire = true;
             // Aiming/firing is now generally OK in Engage, but still not in OffsetApproach/Reposition
             if (CurrentDogfightState == EDogfightState::OffsetApproach || CurrentDogfightState == EDogfightState::Reposition)
             {
                 bShouldAimAndFire = false;
             }
             // Aiming itself (TurnTowards) during Engage will be handled within HandleEngage if needed,
             // OR keep it in Tick's common logic block. Let's keep it common for now.

             if (bShouldAimAndFire)
             {
                 ControlledEnemyShip->TurnTowards(PredictedAimLocation);
                 FVector DirectionToAim = (PredictedAimLocation - ShipLocation).GetSafeNormal();
                 float AimDotProduct = FVector::DotProduct(ShipForward, DirectionToAim);
                 if (AimDotProduct > 0.98f)
                 {
                     ControlledEnemyShip->FireWeapon();
                 }
             }
         }

    } // End of if (Target && bHasLineOfSight)
    else if (Target && !bHasLineOfSight) // Had target, but lost LoS
    {
        UE_LOG(LogSolaraqAI, Warning, TEXT("%s Tick State: === SEARCHING for [%s] (Lost LoS) ==="), *GetName(), *Target->GetName());
        ControlledEnemyShip->TurnTowards(LastKnownTargetLocation);
        ExecuteIdleMovement();
        bIsPerformingBoostTurn = false;
        if(ControlledEnemyShip->IsBoosting()) ControlledEnemyShip->Server_SetAttemptingBoost(false);
        // Reset dogfight state if target is lost
        CurrentDogfightState = EDogfightState::OffsetApproach;
        TimeInCurrentDogfightState = 0.0f;
    }
    else // No Target
    {
         UE_LOG(LogSolaraqAI, Log, TEXT("%s Tick State: === IDLE ==="), *GetName());
        ExecuteIdleMovement();
        bIsPerformingBoostTurn = false;
         // Reset dogfight state if no target
        CurrentDogfightState = EDogfightState::OffsetApproach;
        TimeInCurrentDogfightState = 0.0f;
        if(ControlledEnemyShip->IsBoosting()) ControlledEnemyShip->Server_SetAttemptingBoost(false);
    }
}

void ASolaraqAIController::HandlePerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
    //UE_LOG(LogSolaraqAI, Warning, TEXT("%s Perception Updated. %d actors reported."), *GetName(), UpdatedActors.Num());

    TArray<AActor*> PerceivedActors;
    PerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

    //UE_LOG(LogSolaraqAI, Warning, TEXT("Currently Perceived Actors (Sight): %d"), PerceivedActors.Num());

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

    //UE_LOG(LogSolaraqAI, Warning, TEXT("UpdateTargetActor: Checking %d perceived actors."), PerceivedActors.Num()); // <<< ADDED LOG

    for (AActor* PerceivedActor : PerceivedActors)
    {
        if (!PerceivedActor) continue;

        //UE_LOG(LogSolaraqAI, Warning, TEXT("  -> Checking actor: %s"), *PerceivedActor->GetName()); // <<< ADDED LOG

        // Check Affiliation/Tags
        ETeamAttitude::Type Attitude = GetTeamAttitudeTowards(*PerceivedActor); // <<< CHECK ATTITUDE
        //UE_LOG(LogSolaraqAI, Warning, TEXT("  -> Attitude towards %s: %d"), *PerceivedActor->GetName(), static_cast<int32>(Attitude)); // <<< LOG ATTITUDE

        if (Attitude == ETeamAttitude::Hostile) // <<< ENSURE HOSTILE CHECK
        {
            ASolaraqShipBase* PerceivedShip = Cast<ASolaraqShipBase>(PerceivedActor);
            if (PerceivedShip && PerceivedShip != ControlledEnemyShip && !PerceivedShip->IsDead())
            {
                 //UE_LOG(LogSolaraqAI, Warning, TEXT("  -> Actor %s is a valid Hostile Ship."), *PerceivedActor->GetName()); // <<< ADDED LOG
                float DistSq = FVector::DistSquared(ControlledEnemyShip->GetActorLocation(), PerceivedShip->GetActorLocation());
                if (DistSq < BestTargetDistSq)
                {
                    BestTargetDistSq = DistSq;
                    BestTarget = PerceivedActor;
                    //UE_LOG(LogSolaraqAI, Warning, TEXT("  -> %s is NEW closest valid target."), *BestTarget->GetName()); // <<< ADDED LOG
                }
            }
            else
            {
                 //UE_LOG(LogSolaraqAI, Warning, TEXT("  -> Actor %s is Hostile BUT failed ship check (IsShip=%d, IsSelf=%d, IsDead=%d)"),
                 //     *PerceivedActor->GetName(), IsValid(PerceivedShip), PerceivedShip == ControlledEnemyShip, IsValid(PerceivedShip) ? PerceivedShip->IsDead() : -1 ); // <<< ADDED LOG
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

void ASolaraqAIController::ExecuteIdleMovement()
{
    if (ControlledEnemyShip)
    {
        ControlledEnemyShip->RequestMoveForward(0.0f);
        // No explicit turn command needed, TurnTowards handles turning to target if one exists
        // If truly idle, stopping TurnTowards might be needed if you want it to drift straight
    }
}

void ASolaraqAIController::ExecuteChaseMovement(const FVector& TargetLocation, float DeltaTime)
{
    if (ControlledEnemyShip)
    {
        // Move full speed towards the target
        ControlledEnemyShip->RequestMoveForward(1.0f);
        // Turning is handled by the main Tick logic aiming at PredictedAimLocation
    }
}

void ASolaraqAIController::ExecuteDogfightMovement(AActor* Target, float DeltaTime)
{
    if (!ControlledEnemyShip || !Target) return;

    TimeInCurrentDogfightState += DeltaTime;

    UE_LOG(LogSolaraqAI, Warning, TEXT("%s Dogfight Dispatcher: Current State = %s, Time = %.2f"),
        *GetName(), *UEnum::GetValueAsString(CurrentDogfightState), TimeInCurrentDogfightState);


    switch (CurrentDogfightState)
    {
    case EDogfightState::OffsetApproach:
        HandleOffsetApproach(Target, DeltaTime);
        break;
    case EDogfightState::DriftAim:
        HandleEngage(Target, DeltaTime);
        break;
    case EDogfightState::Reposition:
        HandleReposition(Target, DeltaTime);
        break;
    }
}

void ASolaraqAIController::ExecuteReversalTurnMovement(const FVector& TargetLocation, float AngleToTarget,
    float DeltaTime)
{
    if (ControlledEnemyShip)
    {
        // 1. Activate Boost <<<< REMOVE THIS SECTION >>>>
        /*
        if (!ControlledEnemyShip->IsBoosting())
        {
            UE_LOG(LogSolaraqAI, Warning, TEXT("%s BoostTurn: Activating Boost!"), *GetName());
            ControlledEnemyShip->Server_SetAttemptingBoost(true);
        }
        */
        // <<<< END OF REMOVAL >>>>


        // 2. Turn Towards Target (which is behind) - KEEP
        ControlledEnemyShip->TurnTowards(TargetLocation); // Continue turning aggressively

        // 3. Stop Forward Thrust - KEEP
        ControlledEnemyShip->RequestMoveForward(0.0f); // Stop forward movement during turn

        // 4. Check if Turn is Complete - KEEP
        // Use BoostTurnCompletionAngle or rename the variable if desired
        if (AngleToTarget < BoostTurnCompletionAngle)
        {
            UE_LOG(LogSolaraqAI, Warning, TEXT("%s ReversalTurn: Turn Complete (Angle: %.1f < %.1f)."), *GetName(), AngleToTarget, BoostTurnCompletionAngle);
            // ControlledEnemyShip->Server_SetAttemptingBoost(false); // No longer need to stop boost here
            bIsPerformingBoostTurn = false; // Exit reversal turn state (or use bIsPerformingReversalTurn)
        }
        else
        {
            UE_LOG(LogSolaraqAI, Log, TEXT("%s ReversalTurn: Turning... (Angle: %.1f / %.1f)"), *GetName(), AngleToTarget, BoostTurnCompletionAngle);
        }
    }
    else // Ship became invalid during turn?
    {
        bIsPerformingBoostTurn = false; // or bIsPerformingReversalTurn
    }
}

void ASolaraqAIController::HandleOffsetApproach(AActor* Target, float DeltaTime)
{
    // --- Initial Checks ---
    // Ensure the controlled ship and the target are valid.
    if (!ControlledEnemyShip || !Target)
    {
        // Log an error or warning if pointers are invalid, helps debugging.
        UE_LOG(LogSolaraqAI, Error, TEXT("%s HandleOffsetApproach: Invalid ControlledEnemyShip or Target!"), *GetName());
        // Potentially transition to IDLE or SEARCHING if target is invalid?
        // For now, just returning prevents crashes.
        return;
    }

    // --- Post-Reposition Boost Logic ---
    // Check if we should be boosting because we just finished repositioning.
    if (bShouldBoostOnNextApproach)
    {
        // Activate boost only on the very first frame entering this state after repositioning.
        // TimeInCurrentDogfightState should be <= DeltaTime on the first frame.
        if (TimeInCurrentDogfightState <= DeltaTime)
        {
            UE_LOG(LogSolaraqAI, Warning, TEXT("%s Dogfight: OffsetApproach - Activating Post-Reposition Boost!"), *GetName());
            // Send command to the ship pawn to start boosting.
            ControlledEnemyShip->Server_SetAttemptingBoost(true);
        }

        // Define how long the boost should last during this approach phase.
        // Example: Boost for half the total approach duration. Tune this value.
        const float BoostDuration = OffsetApproachDuration * 0.5f;

        // Check if the boost duration has elapsed.
        if (TimeInCurrentDogfightState > BoostDuration)
        {
            UE_LOG(LogSolaraqAI, Warning, TEXT("%s Dogfight: OffsetApproach - Stopping Post-Reposition Boost (Duration %.2f > %.2f)"),
                   *GetName(), TimeInCurrentDogfightState, BoostDuration);
            // Send command to the ship pawn to stop boosting.
            ControlledEnemyShip->Server_SetAttemptingBoost(false);
            // Clear the flag so we don't try to boost again until after the next reposition.
            bShouldBoostOnNextApproach = false;
        }
    }
    // --- End Boost Logic ---

    // --- Calculate Offset Target Point ---
    const FVector ShipLocation = ControlledEnemyShip->GetActorLocation();
    const FVector TargetLocation = Target->GetActorLocation();
    FVector DirectionToTarget = (TargetLocation - ShipLocation).GetSafeNormal();

    // Decide which side to offset to (left or right) only when first entering the state.
    // This prevents flipping the offset side mid-approach.
    if (TimeInCurrentDogfightState <= DeltaTime) // First frame check
    {
        // Randomly choose -1 (left) or 1 (right).
        CurrentOffsetSide = (FMath::RandBool()) ? 1 : -1;
        UE_LOG(LogSolaraqAI, Log, TEXT("%s Dogfight: Entering OffsetApproach, OffsetSide = %d"), *GetName(), CurrentOffsetSide);
    }

    // Calculate the direction perpendicular to the target direction (using cross product with UpVector for top-down).
    // Ensure it's normalized.
    FVector OffsetDirection = FVector::CrossProduct(DirectionToTarget, FVector::UpVector).GetSafeNormal() * CurrentOffsetSide;
    if (OffsetDirection.IsNearlyZero()) // Safety check if target is directly above/below
    {
        // If direction to target is vertical, use ship's right vector as a fallback offset direction
        OffsetDirection = ControlledEnemyShip->GetActorRightVector() * CurrentOffsetSide;
        UE_LOG(LogSolaraqAI, Warning, TEXT("%s HandleOffsetApproach: Target directly above/below? Using ship's RightVector for offset."), *GetName());
    }


    // Calculate the actual world-space point the AI should move towards.
    // This point is offset from the target's current location.
    CurrentMovementTargetPoint = TargetLocation + (OffsetDirection * DogfightOffsetDistance);

    // --- Movement Execution ---
    // Turn the ship to face the calculated movement target point (the offset point).
    ControlledEnemyShip->TurnTowards(CurrentMovementTargetPoint);
    // Command the ship to apply full forward thrust.
    ControlledEnemyShip->RequestMoveForward(1.0f);

    // Log the target point for debugging.
    UE_LOG(LogSolaraqAI, Log, TEXT("%s Dogfight: OffsetApproach - Moving towards %s"), *GetName(), *CurrentMovementTargetPoint.ToString());

    // --- State Transition Logic ---
    // Check if the allocated time for this approach phase has elapsed.
    if (TimeInCurrentDogfightState >= OffsetApproachDuration)
    {
        // Before transitioning, ensure the boost is turned off if the flag was somehow still active
        // (e.g., if OffsetApproachDuration was shorter than the calculated BoostDuration).
        if (bShouldBoostOnNextApproach)
        {
            UE_LOG(LogSolaraqAI, Warning, TEXT("%s Dogfight: OffsetApproach - State duration ended, ensuring boost is off."), *GetName());
            ControlledEnemyShip->Server_SetAttemptingBoost(false);
            bShouldBoostOnNextApproach = false; // Clear the flag definitively
        }

        // Log the state transition.
        UE_LOG(LogSolaraqAI, Warning, TEXT("%s Dogfight: Transition -> DriftAim (OffsetApproach Duration Ended)"), *GetName());
        // Switch to the next state.
        CurrentDogfightState = EDogfightState::DriftAim;
        // Reset the timer for the new state.
        TimeInCurrentDogfightState = 0.0f;
    }
}

void ASolaraqAIController::HandleEngage(AActor* Target, float DeltaTime)
{
    // --- Initial Checks ---
    if (!ControlledEnemyShip || !Target || !ControlledEnemyShip->GetCollisionAndPhysicsRoot())
    {
        UE_LOG(LogSolaraqAI, Error, TEXT("%s HandleEngage: Invalid ControlledEnemyShip, Target, or PhysicsRoot!"), *GetName());
        return; // Or transition to IDLE/SEARCH?
    }

    // --- Get Current State Info ---
    const FVector ShipLocation = ControlledEnemyShip->GetActorLocation();
    const FVector TargetLocation = Target->GetActorLocation();
    FVector DirectionToTarget = (TargetLocation - ShipLocation).GetSafeNormal();
    FVector ShipVelocity = ControlledEnemyShip->GetCollisionAndPhysicsRoot()->GetPhysicsLinearVelocity();
    float CurrentSpeed = ShipVelocity.Size();

    // --- Movement ---
    // Apply PARTIAL forward thrust consistently to maintain speed
    ControlledEnemyShip->RequestMoveForward(EngageForwardThrustScale); // Use the new parameter

    // Aiming (TurnTowards) is handled by the main Tick loop's common logic block
    // based on PredictedAimLocation when bShouldAimAndFire is true.
    // Firing is also handled by the main Tick loop.

    UE_LOG(LogSolaraqAI, Log, TEXT("%s Dogfight: Engage - Thrust Scale: %.2f, Speed: %.0f, Aiming/Firing Enabled"),
        *GetName(), EngageForwardThrustScale, CurrentSpeed);

    // --- State Transition Logic ---
    bool bTransitionState = false;
    EDogfightState NextState = EDogfightState::Reposition; // Default transition from Engage is usually Reposition
    FString TransitionReason = "";

    // Reason 1: Angle between velocity and target is too wide
    // Check only if moving significantly to avoid spurious transitions at low speed
    if (CurrentSpeed > 100.0f) // Use a threshold slightly above zero
    {
        FVector VelocityDirection = ShipVelocity.GetSafeNormal();
        // Ensure DirectionToTarget is also normalized (should be, but safety check)
        if (!DirectionToTarget.IsNormalized()) DirectionToTarget.Normalize();

        float DotProduct = FVector::DotProduct(VelocityDirection, DirectionToTarget);
        DotProduct = FMath::Clamp(DotProduct, -1.0f, 1.0f); // Clamp for safety before Acos
        float AngleRad = FMath::Acos(DotProduct);
        float AngleDeg = FMath::RadiansToDegrees(AngleRad);

        UE_LOG(LogSolaraqAI, Verbose, TEXT("%s Dogfight: Engage - Angle Check: VelDir vs TargetDir = %.1f deg"), *GetName(), AngleDeg);

        // Use the same threshold name 'DriftAimAngleThreshold' or rename it to 'EngageAngleThreshold'
        if (AngleDeg > DriftAimAngleThreshold)
        {
            bTransitionState = true;
            NextState = EDogfightState::Reposition; // Bad angle triggers reposition
            TransitionReason = FString::Printf(TEXT("Engage Angle Too Wide (%.1f > %.1f)"), AngleDeg, DriftAimAngleThreshold);
        }
    }
    // Reason 2 (Optional Failsafe): Speed *still* dropped too low despite thrust?
    /* else if (CurrentSpeed < MinDriftSpeedThreshold * 0.5f) // Use a lower threshold here maybe
    {
         bTransitionState = true;
         NextState = EDogfightState::OffsetApproach; // Try to recover speed
         TransitionReason = FString::Printf(TEXT("Speed Critically Low (%.0f)"), CurrentSpeed);
    } */


    // --- Perform Transition if Needed ---
    if (bTransitionState)
    {
         UE_LOG(LogSolaraqAI, Warning, TEXT("%s Dogfight: Transition -> %s (%s)"),
                *GetName(), *UEnum::GetValueAsString(NextState), *TransitionReason);
         CurrentDogfightState = NextState;
         TimeInCurrentDogfightState = 0.0f; // Reset timer for the new state
    }
}

void ASolaraqAIController::HandleReposition(AActor* Target, float DeltaTime)
{
    if (!ControlledEnemyShip || !Target) return;

    const FVector ShipLocation = ControlledEnemyShip->GetActorLocation();
    const FVector TargetLocation = Target->GetActorLocation();

    // Calculate direction *away* from the target
    FVector DirectionAway = (ShipLocation - TargetLocation).GetSafeNormal();
    if (DirectionAway.IsNearlyZero()) // If directly on top, pick an arbitrary away direction
    {
        DirectionAway = ControlledEnemyShip->GetActorForwardVector() * -1.0f; // Move backwards
    }

    // Calculate the point to move towards
    CurrentMovementTargetPoint = ShipLocation + (DirectionAway * RepositionDistance); // Move RepositionDistance units away


    // --- Movement ---
    // Face the direction we want to move (away)
    ControlledEnemyShip->TurnTowards(CurrentMovementTargetPoint);
    // Apply full forward thrust
    ControlledEnemyShip->RequestMoveForward(1.0f);

    UE_LOG(LogSolaraqAI, Log, TEXT("%s Dogfight: Reposition - Moving towards %s"), *GetName(), *CurrentMovementTargetPoint.ToString());


    // --- State Transition ---
    if (TimeInCurrentDogfightState >= RepositionDuration)
    {
        UE_LOG(LogSolaraqAI, Warning, TEXT("%s Dogfight: Transition -> OffsetApproach (Reposition Duration Ended). Requesting Boost."), *GetName());
        CurrentDogfightState = EDogfightState::OffsetApproach;
        TimeInCurrentDogfightState = 0.0f; // Reset timer for next state
        bShouldBoostOnNextApproach = true; // **** SET THE FLAG ****
    }
}

float ASolaraqAIController::GetAngleToTarget(const FVector& TargetLocation) const
{
    if (!ControlledEnemyShip) return 180.0f; // Invalid state

    FVector DirectionToTarget = (TargetLocation - ControlledEnemyShip->GetActorLocation()).GetSafeNormal();
    FVector ShipForward = ControlledEnemyShip->GetActorForwardVector();

    // Ensure vectors are normalized
    if (!DirectionToTarget.IsNormalized()) DirectionToTarget.Normalize();
    if (!ShipForward.IsNormalized()) ShipForward.Normalize();

    // Calculate dot product (cosine of angle)
    float Dot = FVector::DotProduct(ShipForward, DirectionToTarget);
    Dot = FMath::Clamp(Dot, -1.0f, 1.0f); // Clamp for safety

    // Get angle in degrees
    float AngleRad = FMath::Acos(Dot);
    return FMath::RadiansToDegrees(AngleRad);
}
