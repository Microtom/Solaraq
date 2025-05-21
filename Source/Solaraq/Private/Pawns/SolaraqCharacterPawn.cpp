// SolaraqCharacterPawn.cpp
#include "Pawns/SolaraqCharacterPawn.h" // Adjust path as necessary
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"
#include "Logging/SolaraqLogChannels.h" // Your log channels

ASolaraqCharacterPawn::ASolaraqCharacterPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // Configure character movement
    GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // Rotation rate
    GetCharacterMovement()->JumpZVelocity = 700.f;
    GetCharacterMovement()->AirControl = 0.35f;
    GetCharacterMovement()->MaxWalkSpeed = 500.f;
    GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

    // For top-down, we usually don't want the controller rotation to affect the character's visual rotation directly
    // if bOrientRotationToMovement is true.
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false; // Set to true if you want mouse to control Yaw directly and not just movement orientation
    bUseControllerRotationRoll = false;

    // Create a camera boom (pulls in towards the player if there is a collision)
    SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArmComponent->SetupAttachment(RootComponent);
    SpringArmComponent->TargetArmLength = 800.0f; // Distance from character
    SpringArmComponent->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f)); // Top-down angle
    SpringArmComponent->bEnableCameraLag = false;
    SpringArmComponent->bInheritPitch = false;
    SpringArmComponent->bInheritYaw = false;
    SpringArmComponent->bInheritRoll = false;
    SpringArmComponent->bDoCollisionTest = false; // Don't want camera to zoom in due to world collision for top-down

    // Create a follow camera
    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName); // Attach camera to end of boom
    CameraComponent->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

    // Set a default mesh (UE Mannequin)
    // You might need to adjust the path depending on your engine version or if you have custom content
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> CharacterMeshAsset(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"));
    if (CharacterMeshAsset.Succeeded())
    {
        GetMesh()->SetSkeletalMesh(CharacterMeshAsset.Object);
        GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
        GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f)); // Orient mesh to face forward
    }
    else
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("ASolaraqCharacterPawn: Failed to find default SKM_Quinn_Simple mesh."));
    }
    
    // Attempt to load animation blueprint
    // Path to the ThirdPerson Animation Blueprint for SKM_Quinn_Simple or SKM_Manny_Simple
    // For UE5.0/5.1+ it's often /Game/Characters/Mannequins/Animations/ABP_Quinn or ABP_Manny
    static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBPClass(TEXT("/Game/Characters/Mannequins/Animations/ABP_Quinn"));
    if (AnimBPClass.Succeeded())
    {
        GetMesh()->SetAnimInstanceClass(AnimBPClass.Class);
    }
    else
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("ASolaraqCharacterPawn: Failed to find default ABP_Quinn."));
    }
}

void ASolaraqCharacterPawn::BeginPlay()
{
    Super::BeginPlay();
    if (bUseCustomCameraLag)
    {
        // If using custom lag, the spring arm's built-in location lag might interfere
        // or produce a double-lag effect. We primarily control its TargetOffset.
        // SpringArmComponent->bEnableCameraLag = false; // Let's test with it ON first.
        // The target offset itself will be lagged by our code.
    }
}

void ASolaraqCharacterPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bUseCustomCameraLag && SpringArmComponent)
    {
        FVector CharacterVelocity = GetVelocity();
        CharacterVelocity.Z = 0;
        FVector CharacterLocation = GetActorLocation();

        const float EffectiveMaxOffsetThreshold = MaxCameraTargetOffset - 1.0f;

        if (!CharacterVelocity.IsNearlyZero()) // CHARACTER IS MOVING
        {
            FVector CurrentMovementDir = CharacterVelocity.GetSafeNormal();

            // Check for significant direction change if we are currently in forced rejoin
            if (bIsInForcedRejoinState)
            {
                // Compare current movement direction with the direction that started the rejoin
                if (!DirectionWhenForcedRejoinStarted.IsZero()) // Ensure it was set
                {
                    float DotProductWithRejoinStartDir = FVector::DotProduct(CurrentMovementDir, DirectionWhenForcedRejoinStarted);
                    if (DotProductWithRejoinStartDir < RejoinDirectionChangeThreshold)
                    {
                        // Significant direction change, break out of forced rejoin
                        bIsInForcedRejoinState = false;
                        TimeAtMaxOffset = 0.0f; // Reset this timer too, to allow new buildup
                        // CurrentCameraTargetOffset will now be handled by the "NOT in forced rejoin" block
                        UE_LOG(LogTemp, Verbose, TEXT("Camera: Broke forced rejoin due to direction change. Dot: %.2f"), DotProductWithRejoinStartDir);
                    }
                }
            }


            FVector IdealLookAheadOffset = CurrentMovementDir * CameraLookAheadFactor;
            FVector ClampedIdealLookAhead = IdealLookAheadOffset.GetClampedToMaxSize(MaxCameraTargetOffset);

            if (bIsInForcedRejoinState) // Re-check, as it might have been changed above
            {
                // FORCED REJOIN IS ACTIVE (and no significant direction change occurred)
                // (Linear or InterpTo logic as before)
                if (RejoinInterpolationMethod == ERejoinInterpolationType::Linear)
                {
                    FVector RejoinDirection = -CurrentCameraTargetOffset.GetSafeNormal();
                    float DistanceToTravelThisFrame = CameraForcedRejoinSpeed_Linear * DeltaTime;
                    if (CurrentCameraTargetOffset.SizeSquared() > FMath::Square(DistanceToTravelThisFrame)) {
                        CurrentCameraTargetOffset += RejoinDirection * DistanceToTravelThisFrame;
                    } else {
                        CurrentCameraTargetOffset = FVector::ZeroVector;
                    }
                } else {
                    CurrentCameraTargetOffset = UKismetMathLibrary::VInterpTo(
                        CurrentCameraTargetOffset, FVector::ZeroVector, DeltaTime, CameraForcedRejoinSpeed_Interp);
                }
            }
            else // NOT in forced rejoin state
            {
                float CurrentOffsetMagnitude = CurrentCameraTargetOffset.Size();
                bool bCurrentlyConsideredAtMax = (CurrentOffsetMagnitude >= EffectiveMaxOffsetThreshold);

                if (bCurrentlyConsideredAtMax)
                {
                    TimeAtMaxOffset += DeltaTime;
                    if (TimeAtMaxOffset >= DelayBeforeForcedRejoin)
                    {
                        bIsInForcedRejoinState = true;
                        DirectionWhenForcedRejoinStarted = CurrentMovementDir; // CAPTURE direction when rejoin starts
                        UE_LOG(LogTemp, Verbose, TEXT("Camera: Initiated forced rejoin. Dir: %s"), *DirectionWhenForcedRejoinStarted.ToString());
                        // First step of rejoin
                        if (RejoinInterpolationMethod == ERejoinInterpolationType::Linear) {
                             FVector RejoinDirection = -CurrentCameraTargetOffset.GetSafeNormal();
                             float DistanceToTravelThisFrame = CameraForcedRejoinSpeed_Linear * DeltaTime;
                             if (CurrentCameraTargetOffset.SizeSquared() > FMath::Square(DistanceToTravelThisFrame)) {
                                CurrentCameraTargetOffset += RejoinDirection * DistanceToTravelThisFrame;
                             } else { CurrentCameraTargetOffset = FVector::ZeroVector; }
                        } else {
                            CurrentCameraTargetOffset = UKismetMathLibrary::VInterpTo( CurrentCameraTargetOffset, FVector::ZeroVector, DeltaTime, CameraForcedRejoinSpeed_Interp);
                        }
                    }
                    else
                    {
                        // AT MAX, BUT STILL IN DELAY PERIOD
                        CurrentCameraTargetOffset = UKismetMathLibrary::VInterpTo(CurrentCameraTargetOffset, ClampedIdealLookAhead, DeltaTime, CustomCameraLagSpeed);
                        CurrentCameraTargetOffset = CurrentCameraTargetOffset.GetSafeNormal() * MaxCameraTargetOffset;
                    }
                }
                else
                {
                    // BUILDING UP OFFSET
                    TimeAtMaxOffset = 0.0f; // Reset timer if we are not at max
                    DirectionWhenForcedRejoinStarted = FVector::ZeroVector; // Clear this if not in rejoin or about to be
                    CurrentCameraTargetOffset = UKismetMathLibrary::VInterpTo(CurrentCameraTargetOffset, ClampedIdealLookAhead, DeltaTime, CustomCameraLagSpeed);
                }
            }
            LastMovementDirection = CurrentMovementDir; // Update for next frame's comparison if needed (though less critical now)
        }
        else // CHARACTER IS STOPPED
        {
            CurrentCameraTargetOffset = UKismetMathLibrary::VInterpTo(CurrentCameraTargetOffset, FVector::ZeroVector, DeltaTime, CameraRecenteringSpeed);
            bIsInForcedRejoinState = false;
            TimeAtMaxOffset = 0.0f;
            LastMovementDirection = FVector::ZeroVector;
            DirectionWhenForcedRejoinStarted = FVector::ZeroVector; // Clear when stopped
        }

        SpringArmComponent->TargetOffset = CurrentCameraTargetOffset;

        // --- DEBUG DRAWING ---
        if (GetWorld())
        {
            DrawDebugSphere(GetWorld(), CharacterLocation, 25.0f, 12, FColor::Green, false, -1.0f, 0, 1.0f);
            FVector SpringArmActualTargetLocation = SpringArmComponent->GetComponentLocation() + SpringArmComponent->TargetOffset;
            DrawDebugSphere(GetWorld(), SpringArmActualTargetLocation, 30.0f, 12, FColor::Blue, false, -1.0f, 0, 1.0f);
            if (CameraComponent) {
                 DrawDebugSphere(GetWorld(), CameraComponent->GetComponentLocation(), 20.0f, 12, FColor::Red, false, -1.0f, 0, 1.0f);
            }
            DrawDebugLine(GetWorld(), CharacterLocation, SpringArmActualTargetLocation, FColor::Yellow, false, -1.0f, 0, 1.0f);
            
            FString DebugText = FString::Printf(TEXT("ForcedRejoin: %s (Dir: %s)\nTimeAtMax: %.2f\nOffsetMag: %.1f\nRejoinType: %s"),
                bIsInForcedRejoinState ? TEXT("TRUE") : TEXT("FALSE"), 
                *DirectionWhenForcedRejoinStarted.ToString(),
                TimeAtMaxOffset, 
                CurrentCameraTargetOffset.Size(), 
                RejoinInterpolationMethod == ERejoinInterpolationType::Linear ? TEXT("Linear") : TEXT("EaseOut"));
            DrawDebugString(GetWorld(), CharacterLocation + FVector(0,0,100), DebugText, nullptr, FColor::White, 0.f, true, 1.f);
        }
    }
    else if (SpringArmComponent) // Custom lag is off
    {
        SpringArmComponent->TargetOffset = FVector::ZeroVector;
        CurrentCameraTargetOffset = FVector::ZeroVector;
        bIsInForcedRejoinState = false;
        TimeAtMaxOffset = 0.0f;
        LastMovementDirection = FVector::ZeroVector;
        DirectionWhenForcedRejoinStarted = FVector::ZeroVector;
    }
}

void ASolaraqCharacterPawn::HandleMoveInput(const FVector2D& MovementVector)
{
    if (Controller != nullptr)
    {
        // Find out which way is forward
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0); // We only care about Yaw for top-down movement direction

        // Get forward vector
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        // Get right vector
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        // Add movement
        AddMovementInput(ForwardDirection, MovementVector.Y);
        AddMovementInput(RightDirection, MovementVector.X);
    }
}

void ASolaraqCharacterPawn::HandleLookInput(const FVector2D& LookAxisVector)
{
    // For a top-down game where character orients to movement, this might not be used directly for character rotation.
    // If you want mouse aiming independent of movement:
    // AddControllerYawInput(LookAxisVector.X);
    // AddControllerPitchInput(LookAxisVector.Y); 
    // And ensure bUseControllerRotationYaw = true on the pawn.
    // For now, we'll assume orientation to movement.
}