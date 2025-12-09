// Characters/Animation/SolaraqAnimInstance.cpp
#include "Pawns/Animations/SolaraqAnimInstance.h" // Adjust path if needed
#include "Pawns/SolaraqCharacterPawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Systems/FishingSubsystem.h" 

void USolaraqAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// Cache the Pawn and Movement Component to avoid casting every frame
	SolaraqCharacter = Cast<ASolaraqCharacterPawn>(TryGetPawnOwner());
	if (SolaraqCharacter)
	{
		MovementComponent = SolaraqCharacter->GetCharacterMovement();
	}
}

void USolaraqAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);

	// Ensure references are valid (might be lost if character dies/respawns)
	if (!SolaraqCharacter || !MovementComponent)
	{
		SolaraqCharacter = Cast<ASolaraqCharacterPawn>(TryGetPawnOwner());
		if(SolaraqCharacter)
		{
			MovementComponent = SolaraqCharacter->GetCharacterMovement();
		}
		else
		{
			return;
		}
	}

	// 1. Calculate Velocity & Speed
	Velocity = MovementComponent->Velocity;
	FVector LateralVelocity = Velocity;
	LateralVelocity.Z = 0.0f;

	// Get the actual physical speed from the component
	const float TargetSpeed = LateralVelocity.Size();

	// Smooth the GroundSpeed toward the TargetSpeed.
	// 15.0f is the InterpSpeed. 
	// Higher = Snappier (Instant), Lower = Smoother (Floaty).
	// 10.0f - 15.0f is usually the sweet spot for top-down responsiveness vs smoothness.
	GroundSpeed = FMath::FInterpTo(GroundSpeed, TargetSpeed, DeltaTime, 15.0f);

	// 2. Determine if we are moving (small threshold to prevent micro-sliding animations)
	bShouldMove = GroundSpeed > 3.0f;

	// 3. Falling State
	bIsFalling = MovementComponent->IsFalling();

	// 4. Custom States from Pawn
	// Note: We use Property Access to read the boolean flags directly from the pawn
	// bIsSprinting is private in Pawn but exposed via getter or standard replication if public.
	// Looking at your Pawn file, bIsSprinting is protected but used in RepNotify. 
    // We can assume we might need a public getter, or just check MaxWalkSpeed vs SprintSpeed.
    // However, usually we add `public: bool IsSprinting() const { return bIsSprinting; }` to the Pawn.
    // For now, let's check max walk speed as a fallback if the getter doesn't exist yet, 
    // BUT you should add `bool IsSprinting() const { return bIsSprinting; }` to your Pawn header.
    
    
    bIsSprinting = SolaraqCharacter->IsSprinting(); 

	bIsSitting = SolaraqCharacter->IsSitting();

	// 5. Fishing State
	if (const UWorld* World = GetWorld())
	{
		if (const UFishingSubsystem* FishingSS = World->GetSubsystem<UFishingSubsystem>())
		{
			bIsFishing = (FishingSS->GetCurrentState() != EFishingState::Idle);
		}
	}
}