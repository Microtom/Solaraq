// SolaraqPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h" // Include if PlayerController handles Team ID
#include "InputActionValue.h" // Include for callback parameter type
#include "SolaraqPlayerController.generated.h"

// Forward Declarations
class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent;
class ASolaraqShipBase; // Forward declare pawn base class
class UUserWidget; 
class UWidgetComponent;


UCLASS()
class SOLARAQ_API ASolaraqPlayerController : public APlayerController, public IGenericTeamAgentInterface // Add team interface if needed here
{
    GENERATED_BODY()

public:
    ASolaraqPlayerController();

    // --- Generic Team Interface (If handling team here instead of Pawn) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team")
    FGenericTeamId TeamId = FGenericTeamId(0); // Player Team ID

    virtual FGenericTeamId GetGenericTeamId() const override;
    // virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override; // Implement if needed
    // --- End Generic Team Interface ---

protected:
    //~ Begin APlayerController Interface
    /** Called when the controller possesses a Pawn. Useful for initial setup. */
    // virtual void OnPossess(APawn* InPawn) override;
    /** Called when the controller unpossesses a Pawn. Useful for cleanup. */
    // virtual void OnUnPossess() override;
    /** Called when the game starts for this controller. Used here to add Input Mapping Context. */
    virtual void BeginPlay() override;
    /** Set up input bindings. */
    virtual void SetupInputComponent() override;
    //~ End APlayerController Interface
    virtual void Tick(float DeltaTime) override;

    // --- Input Assets ---
    // Assign these Input Action Assets in the derived Blueprint Controller (BP_SolaraqPlayerController)

    /** Default Input Mapping Context */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    /** Input Action for forward/backward movement */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    /** Input Action for turning left/right */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> TurnAction;

    /** Input Action for firing weapons */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> FireAction;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> FireMissileAction;
    
    /** Input Action for activating boost */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> BoostAction;

    // Input Action for toggling lock mode
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> ToggleLockAction; 
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> SwitchTargetAction; 

    /** Called for TurnAction input (when completed/released) */
    void HandleTurnCompleted(const FInputActionValue& Value);

    // --- Input Handling Functions ---

    /** Called for MoveAction input */
    void HandleMoveInput(const FInputActionValue& Value);

    /** Called for TurnAction input */
    void HandleTurnInput(const FInputActionValue& Value);

    void HandleFireMissileRequest(const FInputActionValue& Value);
    
    /** Called for FireAction input (when triggered) */
    void HandleFireRequest(); // Renamed for clarity

    /** Called for BoostAction input (when started) */
    void HandleBoostStarted(const FInputActionValue& Value);

    /** Called for BoostAction input (when completed/released) */
    void HandleBoostCompleted(const FInputActionValue& Value);

    void HandleToggleLock();
    
    void HandleSwitchTarget(const FInputActionValue& Value);

    // --- Homing Lock System ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Homing Lock")
    bool bIsHomingLockActive;

    // List of actors currently meeting criteria for targeting
    UPROPERTY(VisibleAnywhere, Category = "Homing Lock")
    TArray<TWeakObjectPtr<AActor>> PotentialHomingTargets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Homing Lock")
    int32 LockedHomingTargetIndex; // Index into PotentialHomingTargets

    // Direct pointer for convenience, updated when index changes
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Homing Lock")
    TWeakObjectPtr<AActor> LockedHomingTargetActor;

    UPROPERTY(EditDefaultsOnly, Category = "Homing Lock")
    float HomingTargetScanRange;

    UPROPERTY(EditDefaultsOnly, Category = "Homing Lock")
    float HomingTargetScanConeAngleDegrees; // e.g., 90.0 for a 90 degree forward cone

    UPROPERTY(EditDefaultsOnly, Category = "Homing Lock")
    float HomingTargetScanInterval; // Time between scans

    FTimerHandle TimerHandle_ScanTargets;

    // --- HUD / Widgets ---
    UPROPERTY(EditDefaultsOnly, Category = "Homing Lock|UI")
    TSubclassOf<UUserWidget> TargetMarkerWidgetClass; // Assign your WBP_TargetMarker

    // Pool or map to manage the screen widgets efficiently
    UPROPERTY() // Keep alive
    TMap<TWeakObjectPtr<AActor>, TObjectPtr<UUserWidget>> TargetMarkerWidgets;

    UPROPERTY() // Keep alive
    TObjectPtr<UUserWidget> LockedTargetMarkerWidgetInstance; // Store the single locked widget instance


    void UpdatePotentialTargets();
    void UpdateTargetWidgets(); // Manages creating/positioning/styling widgets
    void ClearTargetWidgets();
    void SelectTargetByIndex(int32 Index);

private:
    /** Cached pointer to the Enhanced Input Component */
    UPROPERTY()
    TObjectPtr<UEnhancedInputComponent> EnhancedInputComponentRef;

    /** Cached pointer to the controlled ship pawn */
    UPROPERTY() // Cache for efficiency, update OnPossess/OnUnPossess if needed
    TObjectPtr<ASolaraqShipBase> ControlledShipCached;

    /** Helper to get and cache the controlled ship pawn */
    ASolaraqShipBase* GetControlledShip() const;

};

// Implement GetGenericTeamId if needed
inline FGenericTeamId ASolaraqPlayerController::GetGenericTeamId() const { return TeamId; }