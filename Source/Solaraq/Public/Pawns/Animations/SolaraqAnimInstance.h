// Characters/Animation/SolaraqAnimInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SolaraqAnimInstance.generated.h"

class ASolaraqCharacterPawn;
class UCharacterMovementComponent;

UCLASS()
class SOLARAQ_API USolaraqAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaTime) override;

protected:
	// References
	UPROPERTY(BlueprintReadOnly, Category = "Solaraq|References")
	TObjectPtr<ASolaraqCharacterPawn> SolaraqCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "Solaraq|References")
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	// --- State Variables (Read these in your State Machine) ---

	UPROPERTY(BlueprintReadOnly, Category = "Solaraq|Movement")
	FVector Velocity;

	UPROPERTY(BlueprintReadOnly, Category = "Solaraq|Movement")
	float GroundSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "Solaraq|Movement")
	bool bShouldMove;

	UPROPERTY(BlueprintReadOnly, Category = "Solaraq|Movement")
	bool bIsFalling;

	UPROPERTY(BlueprintReadOnly, Category = "Solaraq|Movement")
	bool bIsSprinting;

	UPROPERTY(BlueprintReadOnly, Category = "Solaraq|State")
	bool bIsSitting;
	
	UPROPERTY(BlueprintReadOnly, Category = "Solaraq|State")
	bool bIsFishing;
};