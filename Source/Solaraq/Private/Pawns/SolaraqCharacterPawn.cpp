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
#include "Components/EquipmentComponent.h"
#include "Items/InventoryComponent.h"
#include "Items/ItemToolDataAsset.h"
#include "Logging/SolaraqLogChannels.h" // Your log channels

ASolaraqCharacterPawn::ASolaraqCharacterPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // Configure character movement
    GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 200.0f, 0.0f); // Rotation rate
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

    // Create an inventory component
    InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
    EquipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComponent"));
    
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
    if (InventoryComponent && EquipmentComponent)
    {
        // Load the Data Asset we created in the editor
        UItemToolDataAsset* RodData = LoadObject<UItemToolDataAsset>(nullptr, TEXT("/Game/Items/Tools/FishingRods/BasicFishingRod/DA_BasicFishingRod.DA_BasicFishingRod"));
        if (RodData)
        {
            InventoryComponent->AddItem(RodData, 1);
            EquipmentComponent->EquipItem(RodData);
            UE_LOG(LogTemp, Warning, TEXT("TEST: Gave player a fishing rod."));
        }
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
                        UE_LOG(LogSolaraqMovement, Warning, TEXT("Camera: Broke forced rejoin due to direction change. Dot: %.2f"), DotProductWithRejoinStartDir);
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
                    // Also, smoothly interpolate the direction of the offset towards the current movement direction.
                    // This allows the camera to follow turns even while rejoining.
                    const FVector CurrentOffsetDir = CurrentCameraTargetOffset.GetSafeNormal();
                    const FVector NewOffsetDir = UKismetMathLibrary::VInterpTo(CurrentOffsetDir, CurrentMovementDir, DeltaTime, CustomCameraLagSpeed).GetSafeNormal();
                    
                    // Combine the new direction and new magnitude
                    CurrentCameraTargetOffset = NewOffsetDir * TargetMagnitudeThisFrame;
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

                if (!bCurrentlyConsideredAtMax)
                {
                    // NOT at max offset: Just smoothly interpolate towards the ideal look-ahead position.
                    TimeAtMaxOffset = 0.0f;
                    CurrentCameraTargetOffset = UKismetMathLibrary::VInterpTo(CurrentCameraTargetOffset, ClampedIdealLookAhead, DeltaTime, CustomCameraLagSpeed);
                }
                else // AT max offset:
                {
                    const FVector OffsetDirection = CurrentCameraTargetOffset.GetSafeNormal();
                    const float DotProduct = FVector::DotProduct(CurrentMovementDir, OffsetDirection);

                    // Check if direction has reversed. If so, we need to swing, not orbit or rejoin.
                    if (DotProduct < RejoinDirectionChangeThreshold)
                    {
                        // SWING: Direction reversed. Let the camera swing freely across the center.
                        // DO NOT clamp magnitude here. Reset timer.
                        TimeAtMaxOffset = 0.0f;
                        //UE_LOG(LogSolaraqMovement, Warning, TEXT("Camera: Swinging due to major direction change. Dot: %.2f"), DotProduct);
                        CurrentCameraTargetOffset = UKismetMathLibrary::VInterpTo(CurrentCameraTargetOffset, ClampedIdealLookAhead, DeltaTime, CustomCameraLagSpeed);
                    }
                    else // Direction is still consistent.
                    {
                        TimeAtMaxOffset += DeltaTime;
                        if (TimeAtMaxOffset >= DelayBeforeForcedRejoin)
                        {
                            // REJOIN: Delay is over, start rejoining.
                             bIsInForcedRejoinState = true;
                             DirectionWhenForcedRejoinStarted = CurrentMovementDir;
                             //UE_LOG(LogSolaraqMovement, Warning, TEXT("Camera: Initiated forced rejoin. Dir: %s."), *DirectionWhenForcedRejoinStarted.ToString());
                        }
                        else
                        {
                            // ORBIT: Waiting for rejoin timer. Keep camera at max offset while interpolating direction.
                            CurrentCameraTargetOffset = UKismetMathLibrary::VInterpTo(CurrentCameraTargetOffset, ClampedIdealLookAhead, DeltaTime, CustomCameraLagSpeed);
                            CurrentCameraTargetOffset = CurrentCameraTargetOffset.GetSafeNormal() * MaxCameraTargetOffset;
                        }
                    }
               }
            }
            LastMovementDirection = CurrentMovementDir;
        }
        else // CHARACTER IS STOPPED
        {
            CurrentCameraTargetOffset = UKismetMathLibrary::VInterpTo(CurrentCameraTargetOffset, FVector::ZeroVector, DeltaTime, CameraRecenteringSpeed);
            bIsInForcedRejoinState = false;
            TimeAtMaxOffset = 0.0f;
            LastMovementDirection = FVector::ZeroVector; 
            DirectionWhenForcedRejoinStarted = FVector::ZeroVector;
        }
        
        if (!bIsInForcedRejoinState) {
            DirectionWhenForcedRejoinStarted = FVector::ZeroVector;
        }
        
        // Comprehensive logging for debugging camera behavior
        const FString LogMsg = FString::Printf(TEXT("PawnCam: Vel(%.0f, %.0f) | Offset(%.0f, %.0f) | Rejoin=%d | Time@Max=%.2f | MoveDir(%.1f, %.1f)"), GetVelocity().X, GetVelocity().Y, CurrentCameraTargetOffset.X, CurrentCameraTargetOffset.Y, bIsInForcedRejoinState, TimeAtMaxOffset, CurrentMovementDir.X, CurrentMovementDir.Y);
        //UE_LOG(LogSolaraqMovement, Warning, TEXT("%s"), *LogMsg);
        
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