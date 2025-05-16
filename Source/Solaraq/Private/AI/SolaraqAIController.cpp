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
// #include "AI/SolaraqAIController.h" // Already included above
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
            //UE_LOG(LogSolaraqAI, Warning, TEXT("%s Tick: Controlled Ship Invalid/Dead. Clearing state."), *GetName());
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
    /*UE_LOG(LogSolaraqAI, Log, TEXT("%s TICK CHECK ----> Target: [%s], HasLoS: [%d], BoostTurning: [%d]"),
           *GetName(),
           *GetNameSafe(CurrentTargetActor.Get()), // Use GetNameSafe for TWeakObjectPtr
           bHasLineOfSight,
           bIsPerformingBoostTurn);*/


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
            //UE_LOG(LogSolaraqAI, Error, TEXT("%s Tick State: === STARTING BOOST TURN (Angle: %.1f > %.1f) ==="), *GetName(), AngleToTarget, ReversalAngleThreshold);
            bIsPerformingBoostTurn = true;
            ExecuteReversalTurnMovement(TargetLocation, AngleToTarget, DeltaTime); // Pass calculated values
            CurrentDogfightState = EDogfightState::OffsetApproach; // Reset dogfight state if boosting
            TimeInCurrentDogfightState = 0.0f;
        }
        else if (bIsPerformingBoostTurn)
        {
             //UE_LOG(LogSolaraqAI, Error, TEXT("%s Tick State: === CONTINUING BOOST TURN (Angle: %.1f) ==="), *GetName(), AngleToTarget);
            ExecuteReversalTurnMovement(TargetLocation, AngleToTarget, DeltaTime); // Pass calculated values
        }
        else if (DistanceToTarget <= DogfightRange) // Check Dogfight Range (Uses member variable)
        {
            // Dogfight state is managed internally by ExecuteDogfightMovement now
             ExecuteDogfightMovement(Target, DeltaTime); // Pass the Target actor
        }
        else // Default to Chasing
        {
             //UE_LOG(LogSolaraqAI, Error, TEXT("%s Tick State: === CHASING [%s] (Dist: %.0f > %.0f) ==="), *GetName(), *Target->GetName(), DistanceToTarget, DogfightRange);
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
             
             if (bShouldAimAndFire)
             {
                 ControlledEnemyShip->TurnTowards(PredictedAimLocation, DeltaTime); // MODIFIED: Pass DeltaTime
                 FVector DirectionToAim = (PredictedAimLocation - ShipLocation).GetSafeNormal();
                 float AimDotProduct = FVector::DotProduct(ShipForward, DirectionToAim);
                 if (AimDotProduct > 0.98f) // Firing threshold
                 {
                     ControlledEnemyShip->FireWeapon();
                 }
             }
             // If not bShouldAimAndFire, but still in combat (e.g. OffsetApproach, Reposition),
             // the ship might still need to turn towards its movement target point.
             // This is handled within those respective state functions (HandleOffsetApproach, HandleReposition).
         }

    } // End of if (Target && bHasLineOfSight)
    else if (Target && !bHasLineOfSight) // Had target, but lost LoS
    {
        //UE_LOG(LogSolaraqAI, Warning, TEXT("%s Tick State: === SEARCHING for [%s] (Lost LoS) ==="), *GetName(), *Target->GetName());
        ControlledEnemyShip->TurnTowards(LastKnownTargetLocation, DeltaTime); // MODIFIED: Pass DeltaTime
        ExecuteIdleMovement();
        bIsPerformingBoostTurn = false;
        if(ControlledEnemyShip->IsBoosting()) ControlledEnemyShip->Server_SetAttemptingBoost(false);
        // Reset dogfight state if target is lost
        CurrentDogfightState = EDogfightState::OffsetApproach;
        TimeInCurrentDogfightState = 0.0f;
    }
    else // No Target
    {
         //UE_LOG(LogSolaraqAI, Log, TEXT("%s Tick State: === IDLE ==="), *GetName());
        ExecuteIdleMovement(); // This stops forward movement. Turning stops if no target.
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
        // No explicit turn command needed for idle.
        // If TurnTowards were called with no valid target or current location, it would do nothing.
    }
}

void ASolaraqAIController::ExecuteChaseMovement(const FVector& TargetLocation, float DeltaTime)
{
    if (ControlledEnemyShip)
    {
        // Move full speed towards the target
        ControlledEnemyShip->RequestMoveForward(1.0f);
        // Turning towards PredictedAimLocation is handled by the main Tick's common aiming logic.
        // If chase needs to aim directly at TargetLocation (not PredictedAimLocation),
        // then add: ControlledEnemyShip->TurnTowards(TargetLocation, DeltaTime);
        // But generally, even when chasing, you want to aim where the target *will be* for firing.
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
    case EDogfightState::DriftAim: // Renamed to Engage in some places, let's stick to one. Let's say Engage.
                                   // Make sure EDogfightState enum reflects this if changed.
                                   // Assuming it's still DriftAim for now as per the switch.
        HandleEngage(Target, DeltaTime); // Assuming DriftAim is now Engage
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
        // 1. Activate Boost (This part was removed in thought process, let's ensure it stays out or is re-evaluated)
        // For a sharp reversal, boost might be desired. If so, it should be managed here.
        // If not, ensure boost is OFF or not activated.
        // Current code has boost activation commented out.

        // 2. Turn Towards Target (which is behind)
        ControlledEnemyShip->TurnTowards(TargetLocation, DeltaTime); // MODIFIED: Pass DeltaTime

        // 3. Stop Forward Thrust
        ControlledEnemyShip->RequestMoveForward(0.0f); // Stop forward movement during turn

        // 4. Check if Turn is Complete
        if (AngleToTarget < BoostTurnCompletionAngle) // Uses existing completion angle
        {
            UE_LOG(LogSolaraqAI, Warning, TEXT("%s ReversalTurn: Turn Complete (Angle: %.1f < %.1f)."), *GetName(), AngleToTarget, BoostTurnCompletionAngle);
            bIsPerformingBoostTurn = false; 
            // If boost was activated for the turn, ensure it's turned off here.
            // ControlledEnemyShip->Server_SetAttemptingBoost(false); 
        }
        else
        {
            UE_LOG(LogSolaraqAI, Log, TEXT("%s ReversalTurn: Turning... (Angle: %.1f / %.1f)"), *GetName(), AngleToTarget, BoostTurnCompletionAngle);
        }
    }
    else 
    {
        bIsPerformingBoostTurn = false; 
    }
}

void ASolaraqAIController::HandleOffsetApproach(AActor* Target, float DeltaTime)
{
    // --- Initial Checks ---
    if (!ControlledEnemyShip || !Target)
    {
        UE_LOG(LogSolaraqAI, Error, TEXT("%s HandleOffsetApproach: Invalid ControlledEnemyShip or Target!"), *GetName());
        return;
    }

    // --- Post-Reposition Boost Logic ---
    if (bShouldBoostOnNextApproach)
    {
        if (TimeInCurrentDogfightState <= DeltaTime) // First frame
        {
            UE_LOG(LogSolaraqAI, Warning, TEXT("%s Dogfight: OffsetApproach - Activating Post-Reposition Boost!"), *GetName());
            ControlledEnemyShip->Server_SetAttemptingBoost(true);
        }

        const float BoostDuration = OffsetApproachDuration * 0.5f;
        if (TimeInCurrentDogfightState > BoostDuration)
        {
            UE_LOG(LogSolaraqAI, Warning, TEXT("%s Dogfight: OffsetApproach - Stopping Post-Reposition Boost (Duration %.2f > %.2f)"),
                   *GetName(), TimeInCurrentDogfightState, BoostDuration);
            ControlledEnemyShip->Server_SetAttemptingBoost(false);
            bShouldBoostOnNextApproach = false;
        }
    }
    // --- End Boost Logic ---

    const FVector ShipLocation = ControlledEnemyShip->GetActorLocation();
    const FVector TargetLocation = Target->GetActorLocation();
    FVector DirectionToTarget = (TargetLocation - ShipLocation).GetSafeNormal();

    if (TimeInCurrentDogfightState <= DeltaTime) 
    {
        CurrentOffsetSide = (FMath::RandBool()) ? 1 : -1;
        UE_LOG(LogSolaraqAI, Log, TEXT("%s Dogfight: Entering OffsetApproach, OffsetSide = %d"), *GetName(), CurrentOffsetSide);
    }

    FVector OffsetDirection = FVector::CrossProduct(DirectionToTarget, FVector::UpVector).GetSafeNormal() * CurrentOffsetSide;
    if (OffsetDirection.IsNearlyZero()) 
    {
        OffsetDirection = ControlledEnemyShip->GetActorRightVector() * CurrentOffsetSide;
        UE_LOG(LogSolaraqAI, Warning, TEXT("%s HandleOffsetApproach: Target directly above/below? Using ship's RightVector for offset."), *GetName());
    }
    
    CurrentMovementTargetPoint = TargetLocation + (OffsetDirection * DogfightOffsetDistance);

    // --- Movement Execution ---
    ControlledEnemyShip->TurnTowards(CurrentMovementTargetPoint, DeltaTime); // MODIFIED: Pass DeltaTime
    ControlledEnemyShip->RequestMoveForward(1.0f);

    UE_LOG(LogSolaraqAI, Log, TEXT("%s Dogfight: OffsetApproach - Moving towards %s"), *GetName(), *CurrentMovementTargetPoint.ToString());

    // --- State Transition Logic ---
    if (TimeInCurrentDogfightState >= OffsetApproachDuration)
    {
        if (bShouldBoostOnNextApproach)
        {
            UE_LOG(LogSolaraqAI, Warning, TEXT("%s Dogfight: OffsetApproach - State duration ended, ensuring boost is off."), *GetName());
            ControlledEnemyShip->Server_SetAttemptingBoost(false);
            bShouldBoostOnNextApproach = false; 
        }
        UE_LOG(LogSolaraqAI, Warning, TEXT("%s Dogfight: Transition -> DriftAim (OffsetApproach Duration Ended)"), *GetName());
        CurrentDogfightState = EDogfightState::DriftAim; // Or Engage
        TimeInCurrentDogfightState = 0.0f;
    }
}

void ASolaraqAIController::HandleEngage(AActor* Target, float DeltaTime) // Assuming DriftAim is now Engage
{
    if (!ControlledEnemyShip || !Target || !ControlledEnemyShip->GetCollisionAndPhysicsRoot())
    {
        UE_LOG(LogSolaraqAI, Error, TEXT("%s HandleEngage: Invalid ControlledEnemyShip, Target, or PhysicsRoot!"), *GetName());
        return; 
    }

    const FVector ShipLocation = ControlledEnemyShip->GetActorLocation();
    const FVector TargetLocation = Target->GetActorLocation();
    FVector DirectionToTarget = (TargetLocation - ShipLocation).GetSafeNormal();
    FVector ShipVelocity = ControlledEnemyShip->GetCollisionAndPhysicsRoot()->GetPhysicsLinearVelocity();
    float CurrentSpeed = ShipVelocity.Size();

    ControlledEnemyShip->RequestMoveForward(EngageForwardThrustScale); 

    // Aiming (TurnTowards PredictedAimLocation) is handled by the main Tick loop's common logic.
    // No need to call TurnTowards here unless Engage has a *different* turning target.
    // Firing is also handled by the main Tick loop.

    UE_LOG(LogSolaraqAI, Log, TEXT("%s Dogfight: Engage - Thrust Scale: %.2f, Speed: %.0f, Aiming/Firing Enabled"),
        *GetName(), EngageForwardThrustScale, CurrentSpeed);

    bool bTransitionState = false;
    EDogfightState NextState = EDogfightState::Reposition; 
    FString TransitionReason = "";

    if (CurrentSpeed > 100.0f) 
    {
        FVector VelocityDirection = ShipVelocity.GetSafeNormal();
        if (!DirectionToTarget.IsNormalized()) DirectionToTarget.Normalize();

        float DotProduct = FVector::DotProduct(VelocityDirection, DirectionToTarget);
        DotProduct = FMath::Clamp(DotProduct, -1.0f, 1.0f); 
        float AngleRad = FMath::Acos(DotProduct);
        float AngleDeg = FMath::RadiansToDegrees(AngleRad);

        UE_LOG(LogSolaraqAI, Verbose, TEXT("%s Dogfight: Engage - Angle Check: VelDir vs TargetDir = %.1f deg"), *GetName(), AngleDeg);

        // Using DriftAimAngleThreshold, ensure this UPROPERTY exists or rename if needed.
        if (AngleDeg > DriftAimAngleThreshold) 
        {
            bTransitionState = true;
            NextState = EDogfightState::Reposition; 
            TransitionReason = FString::Printf(TEXT("Engage Angle Too Wide (%.1f > %.1f)"), AngleDeg, DriftAimAngleThreshold);
        }
    }
    
    if (bTransitionState)
    {
         UE_LOG(LogSolaraqAI, Warning, TEXT("%s Dogfight: Transition -> %s (%s)"),
                *GetName(), *UEnum::GetValueAsString(NextState), *TransitionReason);
         CurrentDogfightState = NextState;
         TimeInCurrentDogfightState = 0.0f; 
    }
}

void ASolaraqAIController::HandleReposition(AActor* Target, float DeltaTime)
{
    if (!ControlledEnemyShip || !Target) return;

    const FVector ShipLocation = ControlledEnemyShip->GetActorLocation();
    const FVector TargetLocation = Target->GetActorLocation();

    FVector DirectionAway = (ShipLocation - TargetLocation).GetSafeNormal();
    if (DirectionAway.IsNearlyZero()) 
    {
        DirectionAway = ControlledEnemyShip->GetActorForwardVector() * -1.0f; 
    }
    
    CurrentMovementTargetPoint = ShipLocation + (DirectionAway * RepositionDistance); 

    ControlledEnemyShip->TurnTowards(CurrentMovementTargetPoint, DeltaTime); // MODIFIED: Pass DeltaTime
    ControlledEnemyShip->RequestMoveForward(1.0f);

    UE_LOG(LogSolaraqAI, Log, TEXT("%s Dogfight: Reposition - Moving towards %s"), *GetName(), *CurrentMovementTargetPoint.ToString());

    if (TimeInCurrentDogfightState >= RepositionDuration)
    {
        UE_LOG(LogSolaraqAI, Warning, TEXT("%s Dogfight: Transition -> OffsetApproach (Reposition Duration Ended). Requesting Boost."), *GetName());
        CurrentDogfightState = EDogfightState::OffsetApproach;
        TimeInCurrentDogfightState = 0.0f; 
        bShouldBoostOnNextApproach = true; 
    }
}

float ASolaraqAIController::GetAngleToTarget(const FVector& TargetLocation) const
{
    if (!ControlledEnemyShip) return 180.0f; 

    FVector DirectionToTarget = (TargetLocation - ControlledEnemyShip->GetActorLocation()).GetSafeNormal();
    FVector ShipForward = ControlledEnemyShip->GetActorForwardVector();

    if (!DirectionToTarget.IsNormalized()) DirectionToTarget.Normalize();
    if (!ShipForward.IsNormalized()) ShipForward.Normalize();

    float Dot = FVector::DotProduct(ShipForward, DirectionToTarget);
    Dot = FMath::Clamp(Dot, -1.0f, 1.0f); 

    float AngleRad = FMath::Acos(Dot);
    return FMath::RadiansToDegrees(AngleRad);
}