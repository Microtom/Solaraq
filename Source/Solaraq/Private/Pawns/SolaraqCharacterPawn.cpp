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
#include "Items/SolaraqEquipmentComponent.h" 
#include "Items/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Items/ItemToolDataAsset.h"
#include "Items/ItemArmorDataAsset.h"
#include "Engine/World.h" 
#include "Items/ItemConsumableDataAsset.h"
#include "Items/ItemPickup.h"
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
    SpringArmComponent->TargetArmLength = 1600.0f; // Distance from character
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
    EquipmentComponent = CreateDefaultSubobject<USolaraqEquipmentComponent>(TEXT("EquipmentComponent"));

    NormalMaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
    
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

void ASolaraqCharacterPawn::SetContinuousAiming(bool bEnable)
{
    bShouldContinuouslyAim = bEnable;
    if (bEnable)
    {
        // When we start aiming, disable movement-based rotation.
        GetCharacterMovement()->bOrientRotationToMovement = false;
    }
    else
    {
        // When we stop, re-enable it.
        GetCharacterMovement()->bOrientRotationToMovement = true;
    }
}

void ASolaraqCharacterPawn::StartSprinting()
{
    UE_LOG(LogSolaraqMovement, Warning, TEXT("StartSprinting() CALLED. Sending RPC to server..."));
    Server_SetSprinting(true);
}

void ASolaraqCharacterPawn::StopSprinting()
{
    UE_LOG(LogSolaraqMovement, Warning, TEXT("StopSprinting() CALLED. Sending RPC to server..."));
    Server_SetSprinting(false);
}

void ASolaraqCharacterPawn::DropItem(UItemDataAssetBase* ItemData, int32 Quantity)
{
    if (!ItemData || Quantity <= 0 || !GetWorld() || !DefaultPickupClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("DropItem failed: Invalid parameters or DefaultPickupClass not set in the Character Blueprint."));
        return;
    }

    // 1. Determine a spawn location slightly in front of the player.
    const FVector ActorLocation = GetActorLocation();
    const FVector ActorForward = GetActorForwardVector();
    FVector SpawnLocation = ActorLocation + (ActorForward * 150.0f); // Spawn 1.5m in front.

    // 2. Find the ground below this point using a line trace to avoid spawning mid-air.
    FHitResult HitResult;
    FVector TraceStart = SpawnLocation + FVector(0, 0, 500.0f); // Start trace 5m up
    FVector TraceEnd = SpawnLocation - FVector(0, 0, 500.0f);   // End trace 5m down
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
    {
        // A surface was hit, use the impact point as the final spawn location.
        SpawnLocation = HitResult.ImpactPoint;
    }

    // 3. Spawn the pickup actor.
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AItemPickup* NewPickup = GetWorld()->SpawnActor<AItemPickup>(DefaultPickupClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

    // 4. Initialize the spawned pickup with the correct item data and quantity.
    if (NewPickup)
    {
        NewPickup->ItemData = ItemData;
        NewPickup->Quantity = Quantity;
        // The pickup's own BeginPlay/OnConstruction logic will handle setting the visual mesh.
        UE_LOG(LogTemp, Log, TEXT("Dropped %d x %s into the world at %s."), Quantity, *ItemData->DisplayName.ToString(), *SpawnLocation.ToString());
    }
}

void ASolaraqCharacterPawn::SitDown(USceneComponent* SeatAnchor)
{
    if (!SeatAnchor) return;

    // 1. Disable Movement & Collision
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        GetCharacterMovement()->SetComponentTickEnabled(false); 
    }
    
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 2. ATTACH IMMEDIATELY
    AttachToComponent(SeatAnchor, FAttachmentTransformRules::KeepWorldTransform);

    // 3. Setup Interpolation
    TargetSeatComponent = SeatAnchor;
    
    StartRelativeLocation = GetRootComponent()->GetRelativeLocation();
    StartRelativeRotation = GetRootComponent()->GetRelativeRotation();
    
    SitTransitionAlpha = 0.0f;
    bIsSittingDownTransition = true;
    bIsSitting = true; 
}

void ASolaraqCharacterPawn::BeginSittingSequence(USceneComponent* EntryPoint, USceneComponent* SeatAnchor)
{
    if (!EntryPoint || !SeatAnchor || !SitMontage)
    {
        UE_LOG(LogTemp, Error, TEXT("BeginSittingSequence Failed: Missing EntryPoint, SeatAnchor, or SitMontage."));
        return;
    }

    // 1. Disable Input and Movement
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->SetIgnoreMoveInput(true);
    }
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();

    // 2. Setup Alignment Phase
    // We need to slide from where we stopped (e.g. 5 units away) to the EXACT Entry Point
    AlignStartLoc = GetActorLocation();
    AlignStartRot = GetActorRotation();
    AlignTargetTransform = EntryPoint->GetComponentTransform();
    
    // Flatten Z for alignment to avoid sinking into floor during alignment
    FVector TargetLoc = AlignTargetTransform.GetLocation();
    TargetLoc.Z = AlignStartLoc.Z; 
    AlignTargetTransform.SetLocation(TargetLoc);

    PendingSeatAnchor = SeatAnchor;
    AlignAlpha = 0.0f;
    bIsAligningForSit = true;
    
    // Ensure collision doesn't block the slide
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASolaraqCharacterPawn::StandUp()
{
    if (!bIsSitting) return;

    // Detach first
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    
    if (StandUpMontage)
    {
        UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
        if (AnimInst)
        {
            // Play Stand Up Montage
            AnimInst->Montage_Play(StandUpMontage);

            FOnMontageEnded EndDelegate;
            EndDelegate.BindUObject(this, &ASolaraqCharacterPawn::OnStandUpMontageEnded);
            AnimInst->Montage_SetEndDelegate(EndDelegate, StandUpMontage);
        }
        else
        {
            // Fallback if no AnimInstance
            OnStandUpMontageEnded(nullptr, false);
        }
    }
    else
    {
        // No montage -> Instant snap (Fallback)
        OnStandUpMontageEnded(nullptr, false);
    }
}

void ASolaraqCharacterPawn::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] Pawn BeginPlay Started."));

    // --- FIX: ROBUST COMPONENT RETRIEVAL ---
    // If Blueprint serialization wiped the pointers, find them manually.
    if (!InventoryComponent) 
    {
        InventoryComponent = FindComponentByClass<UInventoryComponent>();
        if (InventoryComponent) UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] Recovered InventoryComponent via FindComponentByClass."));
    }

    if (!EquipmentComponent) 
    {
        EquipmentComponent = FindComponentByClass<USolaraqEquipmentComponent>();
        if (EquipmentComponent) UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] Recovered EquipmentComponent via FindComponentByClass."));
    }
    // ---------------------------------------

    if (InventoryComponent && EquipmentComponent)
    {
        // 1. Fishing Rod
        const TCHAR* RodPath = TEXT("/Game/Items/Tools/FishingRods/BasicFishingRod/DA_BasicFishingRod.DA_BasicFishingRod");
        UItemToolDataAsset* RodData = LoadObject<UItemToolDataAsset>(nullptr, RodPath);
        
        if (RodData)
        {
            UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] LOAD SUCCESS: Rod Data Asset found. Adding to inventory..."));
            InventoryComponent->AddItem(RodData, 1);
        }
        else
        {
            // Right-click your DataAsset in Editor -> Copy Reference to fix this path if it fails
            UE_LOG(LogTemp, Error, TEXT("[DEBUG_INV] LOAD FAILED: Could not find object at path: %s"), RodPath);
        }

        // 2. Apple
        const TCHAR* Berry_Cola_Path = TEXT("/Game/Items/Consumables/DA_Berry_Cola.DA_Berry_Cola");
        UItemConsumableDataAsset* Berry_Cola_Data = LoadObject<UItemConsumableDataAsset>(nullptr, Berry_Cola_Path);
        
        if (Berry_Cola_Data)
        {
            UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] LOAD SUCCESS: Apple Data Asset found. Adding 5..."));
            const int32 UnaddedQuantity = InventoryComponent->AddItem(Berry_Cola_Data, 5);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[DEBUG_INV] LOAD FAILED: Could not find object at path: %s"), Berry_Cola_Path);
        }

        // 3. Helmet
        const TCHAR* HelmetPath = TEXT("/Game/Items/Wearables/Head/BasicHelmet/DA_BasicHelmet.DA_BasicHelmet");
        UItemArmorDataAsset* HelmetData = LoadObject<UItemArmorDataAsset>(nullptr, HelmetPath);
        
        if (HelmetData)
        {
            UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] LOAD SUCCESS: Helmet Data Asset found. Adding..."));
            InventoryComponent->AddItem(HelmetData, 1);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[DEBUG_INV] LOAD FAILED: Could not find object at path: %s"), HelmetPath);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] Pawn BeginPlay Finished. Inventory Total Items: %d"), InventoryComponent->GetPlacedItems().Num());
    }
    else
    {
        // Specific Error Logging
        if (!InventoryComponent) UE_LOG(LogTemp, Error, TEXT("[DEBUG_INV] CRITICAL: InventoryComponent is still NULL after recovery attempt!"));
        if (!EquipmentComponent) UE_LOG(LogTemp, Error, TEXT("[DEBUG_INV] CRITICAL: EquipmentComponent is still NULL after recovery attempt!"));
    }
}

void ASolaraqCharacterPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASolaraqCharacterPawn, bIsSprinting);
}

void ASolaraqCharacterPawn::OnRep_IsSprinting()
{
    if (bIsSprinting)
    {
        GetCharacterMovement()->MaxWalkSpeed = SprintMaxWalkSpeed;
    }
    else
    {
        GetCharacterMovement()->MaxWalkSpeed = NormalMaxWalkSpeed;
    }
}

void ASolaraqCharacterPawn::Server_SetSprinting_Implementation(bool bNewSprintingState)
{
    if (bIsSprinting != bNewSprintingState)
    {
        bIsSprinting = bNewSprintingState;
        // Call OnRep on the server for immediate effect (for host/listen server)
        OnRep_IsSprinting();
    }
}

void ASolaraqCharacterPawn::OnSitMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage == SitMontage)
    {
        // PHASE 2: LOCKED IN SEAT
        bIsSitting = true; // Tells AnimBP to switch to "Sitting Loop"
        
        // Attach to the chair so if the chair moves (e.g. on a ship), we move with it.
        // Important: KeepWorldTransform because the Root Motion moved us to the correct spot.
        if (PendingSeatAnchor)
        {
            AttachToComponent(PendingSeatAnchor, FAttachmentTransformRules::KeepWorldTransform);
        }
        
        // Re-enable Input (but not movement, since bIsSitting blocks it in Controller)
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            PC->SetIgnoreMoveInput(false);
        }
    }
}

void ASolaraqCharacterPawn::OnStandUpMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    bIsSitting = false;
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        GetCharacterMovement()->SetComponentTickEnabled(true);
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
    else if (bShouldContinuouslyAim)
    {
        // If we are continuously aiming, we keep bOrientRotationToMovement false
        GetCharacterMovement()->bOrientRotationToMovement = false;

        // Get the current aim direction from the cursor
        const FVector AimDirection = GetAimDirection();
        const FRotator AimRotation = AimDirection.Rotation();

        // We only care about Yaw for the character's rotation
        const FRotator CurrentRotation = GetActorRotation();
        const FRotator TargetRotation = FRotator(0.f, AimRotation.Yaw, 0.f);

        // Interpolate smoothly towards the target rotation
        const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, AimTurnInterpSpeed);
        SetActorRotation(NewRotation);

        // We also need to update the ProgrammaticTargetRotation for the camera to use.
        ProgrammaticTargetRotation = NewRotation; 
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

    // --- PHASE 1: ALIGNMENT ---
    if (bIsAligningForSit)
    {
        AlignAlpha += DeltaTime * 5.0f; // 0.2 seconds to align (Fast slide)
        AlignAlpha = FMath::Clamp(AlignAlpha, 0.0f, 1.0f);

        FVector NewLoc = FMath::Lerp(AlignStartLoc, AlignTargetTransform.GetLocation(), AlignAlpha);
        FRotator NewRot = FMath::Lerp(AlignStartRot, AlignTargetTransform.GetRotation().Rotator(), AlignAlpha);

        SetActorLocationAndRotation(NewLoc, NewRot);

        if (AlignAlpha >= 1.0f)
        {
            // Alignment Complete -> Start Montage
            bIsAligningForSit = false;
            
            UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
            if (AnimInst)
            {
                // Play the Root Motion Montage
                AnimInst->Montage_Play(SitMontage);
                
                // Bind to the end of the montage to trigger the "Loop" state
                FOnMontageEnded EndDelegate;
                EndDelegate.BindUObject(this, &ASolaraqCharacterPawn::OnSitMontageEnded);
                AnimInst->Montage_SetEndDelegate(EndDelegate, SitMontage);
            }
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