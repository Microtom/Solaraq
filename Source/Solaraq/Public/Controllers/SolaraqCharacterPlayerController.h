// SolaraqCharacterPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "Controllers/SolaraqBasePlayerController.h" // Inherit from our new base
#include "Pawns/SolaraqCharacterPawn.h"
#include "SolaraqCharacterPlayerController.generated.h"

struct FInputActionValue;
// Forward Declarations
class UInputMappingContext;
class UInputAction;
class ASolaraqCharacterPawn;

UCLASS()
class SOLARAQ_API ASolaraqCharacterPlayerController : public ASolaraqBasePlayerController
{
    GENERATED_BODY()

public:
    ASolaraqCharacterPlayerController();

    // --- Pawn Getter ---
    ASolaraqCharacterPawn* GetControlledCharacter() const;

    /** Creates and shows the fishing HUD widget. */
    void ShowFishingHUD();
    /** Hides and cleans up the fishing HUD widget. */
    void HideFishingHUD();
    
protected:
    //~ Begin ASolaraqBasePlayerController Interface (Overrides)
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void SetupInputComponent() override;
    // Tick might not be needed if character PC is simple, but override for completeness
    virtual void Tick(float DeltaTime) override;
    virtual void OnRep_Pawn() override;
    //~ End ASolaraqBasePlayerController Interface

    // --- Input Assets ---
    /** Input Mapping Context for Character Controls */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    TObjectPtr<UInputMappingContext> IMC_CharacterControls;

    // --- Character Input Actions ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    TObjectPtr<UInputAction> CharacterMoveAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    TObjectPtr<UInputAction> PrimaryUseAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    TObjectPtr<UInputAction> SecondaryUseAction;

    // We now use a single action for both tapping and holding the pointer.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    TObjectPtr<UInputAction> PointerMoveAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    TObjectPtr<UInputAction> CameraZoomAction; // For the mouse wheel

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    TObjectPtr<UInputAction> ToggleFishingModeAction;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Camera")
    TObjectPtr<UCurveFloat> CameraZoomCurve;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Camera")
    float MinZoomLength = 300.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Camera")
    float MaxZoomLength = 2000.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Camera")
    float FishingModeZoomLength = 1700.f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Camera")
    float ZoomStepAmount = 100.f; // How much each mouse wheel tick changes the target zoom

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Camera")
    float ZoomInterpSpeed = 5.f; // How smoothly the camera zooms in/out

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Camera")
    float RotationInterpSpeed = 5.f; // How smoothly the camera rotates to match the zoom

    FVector TargetCameraOffset;
    
    UPROPERTY(EditAnywhere, Category = "Solaraq|Camera")
    float CameraOffsetInterpSpeed = 3.f;

    float PreFishingZoomLength;
    bool bWasInFishingMode_LastFrame = false;

    // --- Custom Camera Lag ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Camera|Custom Lag")
    bool bUseCustomCameraLag = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
    float CustomCameraLagSpeed = 2.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
    float CameraLookAheadFactor = 150.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
    float CameraRecenteringSpeed = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
    float MaxCameraTargetOffset = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
    ERejoinInterpolationType RejoinInterpolationMethod = ERejoinInterpolationType::Linear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
    float CameraForcedRejoinSpeed_Interp = 1.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag", ClampMin = "0.0"))
    float CameraForcedRejoinSpeed_Linear = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag", ClampMin = "-1.0", ClampMax = "1.0"))
    float RejoinDirectionChangeThreshold = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Camera|Custom Lag", meta = (EditCondition = "bUseCustomCameraLag"))
    float DelayBeforeForcedRejoin = 0.25f;
    
    /** The class of the fishing HUD widget to create. Assign this in the PlayerController Blueprint. */
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UUserWidget> FishingHUDWidgetClass;

    /** A pointer to the instance of the fishing HUD, so we can show/hide it. */
    UPROPERTY()
    TObjectPtr<UUserWidget> FishingHUDWidgetInstance;
    
    // --- Input Handling Functions (Character & Shared Handlers) ---
    void HandlePointerMove(const FInputActionValue& Value);
    void HandleCharacterInteractInput(); // Specific handler for character interaction
    void HandlePrimaryUseStarted();
    void HandlePrimaryUseCompleted(); // For 'Release' triggers
    void HandleSecondaryUseStarted();
    void HandleSecondaryUseCompleted();
    void HandleToggleFishingMode();
    void HandleCharacterMoveInput(const FInputActionValue& Value);
    void HandleCameraZoom(const FInputActionValue& Value);
    void MoveToDestination(const FVector& Destination);
    
private:
    // No longer need specific PossessedCharacterPawn, GetControlledCharacter() will cast GetPawn()
    void ApplyCharacterInputMappingContext();

    FVector CachedDestination;
    float LastMoveRequestTime = 0.f;
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Input|Character")
    float MoveRequestDebounceTime = 0.2f;

    float TargetZoomLength;

    // --- Camera Lag State Variables ---
    FVector CurrentCameraTargetOffset; 
    bool bIsInForcedRejoinState = false;
    float TimeAtMaxOffset = 0.0f;
    FVector LastMovementDirection = FVector::ZeroVector;
    FVector DirectionWhenForcedRejoinStarted = FVector::ZeroVector;
    
	
    bool bIsMaxOffsetReached = false;        // True if current offset is at/near max
};