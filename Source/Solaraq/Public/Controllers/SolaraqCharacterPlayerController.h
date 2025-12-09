// SolaraqCharacterPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "Controllers/SolaraqBasePlayerController.h" // Inherit from our new base
#include "Pawns/SolaraqCharacterPawn.h"
#include "Actors/Interactables/SolaraqInteractableChair.h"
#include "SolaraqCharacterPlayerController.generated.h"

class ASolaraqContainerBase;
class USolaraqEquipmentWindowWidget;
class USolaraqHUDWidget;
class USolaraqInventoryWindowWidget;
struct FInputActionValue;
// Forward Declarations
class UInputMappingContext;
class UInputAction;
class USolaraqContainerWindowWidget;
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

    /** 
     * Generic function called by ANY interactable (Chair, Bed, Chest).
     * The controller will walk to 'TargetLocation' and then call Interact() on 'TargetActor'.
     */
    void RequestMoveToInteract(AActor* TargetActor, FVector TargetLocation, float AcceptanceRadius = 10.0f);

    /** Called by the Container Actor when interaction succeeds. */
    void OpenContainerInventory(ASolaraqContainerBase* Container);
    
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

    
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|UI")
    TSubclassOf<USolaraqHUDWidget> MainHUDWidgetClass;

    UPROPERTY(Transient) // Good practice to mark runtime-only instances as Transient
    TObjectPtr<USolaraqHUDWidget> MainHUDWidgetInstance;
    
    
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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    TObjectPtr<UInputAction> SprintAction;
    
    // We now use a single action for both tapping and holding the pointer.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    TObjectPtr<UInputAction> PointerMoveAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    TObjectPtr<UInputAction> ToggleEquipmentWindowAction;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    TObjectPtr<UInputAction> CameraZoomAction; // For the mouse wheel

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    TObjectPtr<UInputAction> ToggleFishingModeAction;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Camera")
    TObjectPtr<UCurveFloat> CameraZoomCurve;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Camera")
    float MinZoomLength = 300.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Camera")
    float MaxZoomLength = 2000.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Camera")
    float FishingModeZoomLength = 1700.f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Camera")
    float ZoomStepAmount = 100.f; // How much each mouse wheel tick changes the target zoom

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Camera")
    float ZoomInterpSpeed = 5.f; // How smoothly the camera zooms in/out

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Camera")
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
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|UI")
    TSubclassOf<UUserWidget> FishingHUDWidgetClass;

    /** A pointer to the instance of the fishing HUD, so we can show/hide it. */
    UPROPERTY()
    TObjectPtr<UUserWidget> FishingHUDWidgetInstance;

    /** Widget class for the CHARACTER's inventory grid. */
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|UI")
    TSubclassOf<USolaraqInventoryWindowWidget> CharacterInventoryWidgetClass;

    /** Instance of the character inventory grid widget. */
    UPROPERTY()
    TObjectPtr<USolaraqInventoryWindowWidget> CharacterInventoryWidgetInstance;

    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|UI")
    TSubclassOf<USolaraqEquipmentWindowWidget> EquipmentWindowWidgetClass;
    
    UPROPERTY()
    TObjectPtr<USolaraqEquipmentWindowWidget> EquipmentWindowInstance;

    /** Widget class for the CONTAINER inventory window. */
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|UI")
    TSubclassOf<USolaraqContainerWindowWidget> ContainerWindowWidgetClass;

    /** Active instance of the container window. */
    UPROPERTY()
    TObjectPtr<USolaraqContainerWindowWidget> ContainerWindowInstance;
    
    // --- Input Handling Functions (Character & Shared Handlers) ---
    void HandlePointerMove(const FInputActionValue& Value);
    void HandleCharacterInteractInput(); // Specific handler for character interaction
    void HandlePrimaryUseStarted();
    void HandlePrimaryUseCompleted(); // For 'Release' triggers
    void HandleSecondaryUseStarted();
    void HandleSecondaryUseCompleted();
    void HandleToggleFishingMode();
    void HandleCharacterToggleInventory();
    void HandleToggleEquipmentWindow();
    void HandleCharacterMoveInput(const FInputActionValue& Value);
    void HandleCameraZoom(const FInputActionValue& Value);
    void MoveToDestination(const FVector& Destination);
    void HandleSprintStarted(const FInputActionValue& Value);
    void HandleSprintCompleted(const FInputActionValue& Value);

    UFUNCTION()
    void OnInventoryClosedByUI();

    UFUNCTION()
    void OnEquipmentClosedByUI();

    UFUNCTION()
    void OnContainerClosedByUI();
    
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

    // --- Inventory Window State Management ---

    /** The last known position of the inventory window, to be persisted across toggles. */
    UPROPERTY()
    FVector2D LastInventoryPosition;
    UPROPERTY()
    FVector2D LastEquipmentPosition;

    /** Flag to check if we have a custom position saved, or if we should use the default centered position. */
    bool bIsInventoryPositionSet = false;
    bool bIsEquipmentPositionSet;
    
    void CreateHUD();
	
    bool bIsMaxOffsetReached = false;        // True if current offset is at/near max

    // We store the generic Actor, not a specific class
    UPROPERTY()
    TObjectPtr<AActor> PendingInteractableActor;

    FVector PendingInteractionLocation;
    float PendingInteractionRadius;
    bool bIsAutoNavigatingToInteract = false;
};