// SolaraqCharacterPawn.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SolaraqCharacterPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
// class UInputMappingContext; // Not needed in pawn.h
// class UInputAction; // Not needed in pawn.h
// struct FInputActionValue; // Not needed in pawn.h

UENUM(BlueprintType)
enum class ERejoinInterpolationType : uint8
{
	InterpTo UMETA(DisplayName = "Ease Out (VInterpTo)"),
	Linear   UMETA(DisplayName = "Linear Speed")
};

UCLASS()
class SOLARAQ_API ASolaraqCharacterPawn : public ACharacter
{
	GENERATED_BODY()

public:
	ASolaraqCharacterPawn();


protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArmComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> CameraComponent;

    // --- Custom Camera Control ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Custom Lag")
    bool bUseCustomCameraLag = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
    float CustomCameraLagSpeed = 5.0f;

    // Offset in the direction opposite to velocity, making camera look "ahead"
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
    float CameraLookAheadFactor = 150.0f; 

    // How quickly the camera returns to center when character stops
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
    float CameraRecenteringSpeed = 2.0f;

    // Max distance the camera target can be offset
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
    float MaxCameraTargetOffset = 150.0f;

	// Speed at which the camera target offset "shrinks" back towards the player
	// when it has reached max offset and player is still moving.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
	float CameraForcedRejoinSpeed = 1.0f; // Slower than CustomCameraLagSpeed

	// (Optional) A small delay once max offset is reached before forced rejoin begins.
	// If 0, rejoin starts immediately once max offset is hit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
	float DelayBeforeForcedRejoin = 0.25f;

	// Speed at which the camera target offset "shrinks" back towards the player
	// when it has reached max offset and player is still moving. (Non-linear interp speed)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag", DisplayName = "Forced Rejoin Interp Speed (Old)"))
	float CameraForcedRejoinSpeed_Interp = 1.0f; 

	// NEW: Linear speed (units per second) for forced rejoin
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag", ClampMin = "0.0"))
	float CameraForcedRejoinSpeed_Linear = 20.0f; // e.g., offset shrinks by 50 units per second

	// NEW: Stores the movement direction that was active when forced rejoin started
	FVector DirectionWhenForcedRejoinStarted = FVector::ZeroVector; 

	// NEW: Threshold for what's considered a "significant" direction change (dot product)
	// Lower value = more sensitive to direction changes. 1.0 = same direction, 0.0 = 90 degrees, -1.0 = opposite.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag", ClampMin = "-1.0", ClampMax = "1.0"))
	float RejoinDirectionChangeThreshold = 0.1f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
	ERejoinInterpolationType RejoinInterpolationMethod = ERejoinInterpolationType::Linear;
	
private:
    FVector CurrentCameraTargetOffset; // The current offset we are applying to the spring arm target

	bool bIsInForcedRejoinState = false;
	
	bool bIsMaxOffsetReached = false;        // True if current offset is at/near max
	float TimeAtMaxOffset = 0.0f;           // Timer for DelayBeforeForcedRejoin
	FVector LastMovementDirection = FVector::ZeroVector; // To detect if movement direction is consistent
public:
	virtual void Tick(float DeltaTime) override;

	void HandleMoveInput(const FVector2D& MovementVector);
	void HandleLookInput(const FVector2D& LookAxisVector);

	FORCEINLINE class USpringArmComponent* GetSpringArmComponent() const { return SpringArmComponent; }
	FORCEINLINE class UCameraComponent* GetCameraComponent() const { return CameraComponent; }
};