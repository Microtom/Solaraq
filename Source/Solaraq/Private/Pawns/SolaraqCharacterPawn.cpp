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
#include "Systems/FishingSubsystem.h"

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

FVector ASolaraqCharacterPawn::GetAimDirection() const
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC)
    {
        // Fallback to the pawn's forward vector if we have no controller
        return GetActorForwardVector();
    }

    // Get the intersection of the cursor with a plane at the character's height
    FVector WorldLocation, WorldDirection;
    if (PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
    {
        FPlane GroundPlane(GetActorLocation(), FVector::UpVector);
        FVector Intersection = FMath::LinePlaneIntersection(
            WorldLocation,
            WorldLocation + WorldDirection * 10000.f, // A very long line
            GroundPlane
        );

        // Calculate direction from pawn to the intersection point and ignore Z
        FVector Direction = Intersection - GetActorLocation();
        Direction.Z = 0;
        return Direction.GetSafeNormal();
    }

    // Fallback if deprojection fails
    return GetActorForwardVector();
}

FRotator ASolaraqCharacterPawn::GetTargetAimingRotation() const
{
    return ProgrammaticTargetRotation;
}

void ASolaraqCharacterPawn::StartSmoothTurn(const FRotator& TargetRotation)
{
    if (bIsProgrammaticallyTurning)
    {
        UE_LOG(LogTemp, Warning, TEXT("Pawn::StartSmoothTurn - Already turning. Updating target rotation."));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Pawn::StartSmoothTurn - Initiating new turn."));
    }

    // We only care about the Yaw for a top-down game character turn.
    ProgrammaticTargetRotation = FRotator(0.f, TargetRotation.Yaw, 0.f);
    bIsProgrammaticallyTurning = true;
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

    // --- PROGRAMMATIC TURNING LOGIC ---
    if (bIsProgrammaticallyTurning)
    {
        // While we are in a forced turn, we disable movement-based rotation.
        GetCharacterMovement()->bOrientRotationToMovement = false;

        const FRotator CurrentRotation = GetActorRotation();
        
        // Interpolate smoothly towards the target rotation
        const FRotator NewRotation = FMath::RInterpTo(CurrentRotation.GetNormalized(), ProgrammaticTargetRotation.GetNormalized(), DeltaTime, AimTurnInterpSpeed);

        UE_LOG(LogTemp, Warning, TEXT("Pawn::Tick - Turning. Current Yaw: %.2f, Target Yaw: %.2f, New Yaw: %.2f"), CurrentRotation.Yaw, ProgrammaticTargetRotation.Yaw, NewRotation.Yaw);

        SetActorRotation(NewRotation);

        // Check if the turn is complete (with a small tolerance)
        if (FMath::IsNearlyEqual(NewRotation.Yaw, ProgrammaticTargetRotation.Yaw, 0.5f))
        {
            UE_LOG(LogTemp, Warning, TEXT("Pawn::Tick - Turn complete. Finalizing rotation and stopping turn."));
            bIsProgrammaticallyTurning = false;
            SetActorRotation(ProgrammaticTargetRotation); // Snap to final rotation
        }
    }
    else
    {
        // When not in a forced turn, return to normal movement-based rotation.
        GetCharacterMovement()->bOrientRotationToMovement = true;
    }
    
    if (UFishingSubsystem* FishingSS = GetWorld()->GetSubsystem<UFishingSubsystem>())
    {
        if (FishingSS->GetCurrentState() != EFishingState::Idle)
        {
            // Draw the debug circle on the ground
            DrawDebugCircle(
                GetWorld(),
                GetActorLocation(),
                FishingCameraRadius,
                32, // Segments
                FColor::Cyan,
                false, // Persistent
                -1, // Lifetime
                0, // Depth Priority
                2.f, // Thickness
                FVector(1,0,0), // Y-axis
                FVector(0,1,0), // X-axis
                false
            );
            
            // Draw the aiming line
            const FVector AimDir = GetAimDirection();
            DrawDebugLine(
                GetWorld(),
                GetActorLocation(),
                GetActorLocation() + AimDir * FishingCameraRadius,
                FColor::Red,
                false,
                -1,
                0,
                5.f
            );
        }
    }

    bool bIsFishingActive = false;
    if (UFishingSubsystem* FishingSS = GetWorld()->GetSubsystem<UFishingSubsystem>())
    {
        bIsFishingActive = (FishingSS->GetCurrentState() != EFishingState::Idle);
    }
    
    if (bUseCustomCameraLag && SpringArmComponent && !bIsFishingActive)
    {
        // The entire block of your existing camera look-ahead logic goes here.
        // It will now be completely skipped when any fishing state is active.
        
        FVector CharacterVelocity = GetVelocity();
        // ... all of your existing custom camera lag code remains here
        // ...
        // SpringArmComponent->TargetOffset = CurrentCameraTargetOffset;
    }
    else if (SpringArmComponent) 
    {
        // If fishing IS active, or if custom lag is disabled, we must ensure
        // the pawn's logic doesn't interfere. We let the PlayerController
        // handle the TargetOffset exclusively.
        // The 'else if' for when bUseCustomCameraLag is false can remain,
        // as it correctly resets the values.
        if (!bUseCustomCameraLag)
        {
            SpringArmComponent->TargetOffset = FVector::ZeroVector;
            CurrentCameraTargetOffset = FVector::ZeroVector;
            bIsInForcedRejoinState = false;
            TimeAtMaxOffset = 0.0f;
            LastMovementDirection = FVector::ZeroVector;
            DirectionWhenForcedRejoinStarted = FVector::ZeroVector;
        }
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