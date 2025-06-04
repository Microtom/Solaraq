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

        FVector CurrentMovementDir = FVector::ZeroVector; // <<<< DECLARE AND INITIALIZE HERE

        if (!CharacterVelocity.IsNearlyZero()) // CHARACTER IS MOVING
        {
            CurrentMovementDir = CharacterVelocity.GetSafeNormal(); // <<<< ASSIGN VALUE HERE

            // Check for significant direction change if we are currently in forced rejoin
            if (bIsInForcedRejoinState)
            {
                if (!DirectionWhenForcedRejoinStarted.IsZero())
                {
                    float DotProductWithRejoinStartDir = FVector::DotProduct(CurrentMovementDir, DirectionWhenForcedRejoinStarted);
                    if (DotProductWithRejoinStartDir < RejoinDirectionChangeThreshold)
                    {
                        bIsInForcedRejoinState = false;
                        TimeAtMaxOffset = 0.0f;
                        UE_LOG(LogTemp, Verbose, TEXT("Camera: Broke forced rejoin due to direction change. Dot: %.2f"), DotProductWithRejoinStartDir);
                    }
                }
            }

            FVector IdealLookAheadOffset = CurrentMovementDir * CameraLookAheadFactor;
            FVector ClampedIdealLookAhead = IdealLookAheadOffset.GetClampedToMaxSize(MaxCameraTargetOffset);

            if (bIsInForcedRejoinState) 
            {
                // FORCED REJOIN IS ACTIVE
                if (RejoinInterpolationMethod == ERejoinInterpolationType::Linear)
                {
                    float CurrentMagnitude = CurrentCameraTargetOffset.Size();
                    float TargetMagnitudeThisFrame = FMath::Max(0.f, CurrentMagnitude - (CameraForcedRejoinSpeed_Linear * DeltaTime));
                    CurrentCameraTargetOffset = CurrentMovementDir * TargetMagnitudeThisFrame;
                    // if (TargetMagnitudeThisFrame <= 0.0f) { } // No specific action needed if it hits zero here
                }
                else 
                {
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
                        DirectionWhenForcedRejoinStarted = CurrentMovementDir;
                        UE_LOG(LogTemp, Verbose, TEXT("Camera: Initiated forced rejoin. Dir: %s"), *DirectionWhenForcedRejoinStarted.ToString());
                        
                        if (RejoinInterpolationMethod == ERejoinInterpolationType::Linear) {
                             float CurrentMagnitude = CurrentCameraTargetOffset.Size();
                             float TargetMagnitudeThisFrame = FMath::Max(0.f, CurrentMagnitude - (CameraForcedRejoinSpeed_Linear * DeltaTime));
                             CurrentCameraTargetOffset = CurrentMovementDir * TargetMagnitudeThisFrame;
                        } else {
                            CurrentCameraTargetOffset = UKismetMathLibrary::VInterpTo( CurrentCameraTargetOffset, FVector::ZeroVector, DeltaTime, CameraForcedRejoinSpeed_Interp);
                        }
                    }
                    else
                    {
                        CurrentCameraTargetOffset = UKismetMathLibrary::VInterpTo(CurrentCameraTargetOffset, ClampedIdealLookAhead, DeltaTime, CustomCameraLagSpeed);
                        CurrentCameraTargetOffset = CurrentCameraTargetOffset.GetSafeNormal() * MaxCameraTargetOffset;
                    }
                }
                else
                {
                    TimeAtMaxOffset = 0.0f;
                    CurrentCameraTargetOffset = UKismetMathLibrary::VInterpTo(CurrentCameraTargetOffset, ClampedIdealLookAhead, DeltaTime, CustomCameraLagSpeed);
                }
            }
            LastMovementDirection = CurrentMovementDir;
        }
        else // CHARACTER IS STOPPED
        {
            // CurrentMovementDir remains ZeroVector as initialized
            CurrentCameraTargetOffset = UKismetMathLibrary::VInterpTo(CurrentCameraTargetOffset, FVector::ZeroVector, DeltaTime, CameraRecenteringSpeed);
            bIsInForcedRejoinState = false;
            TimeAtMaxOffset = 0.0f;
            LastMovementDirection = FVector::ZeroVector; // Already ZeroVector from CurrentMovementDir
            DirectionWhenForcedRejoinStarted = FVector::ZeroVector;
        }
        
        if (!bIsInForcedRejoinState) {
            DirectionWhenForcedRejoinStarted = FVector::ZeroVector;
        }

        SpringArmComponent->TargetOffset = CurrentCameraTargetOffset;
        
    }
    else if (SpringArmComponent) 
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