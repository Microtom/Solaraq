// SolaraqPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h" // Include if PlayerController handles Team ID
#include "InputActionValue.h"         // Include for callback parameter type
#include "SolaraqPlayerController.generated.h"

// Forward Declarations
class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent;
class ASolaraqShipBase;
class ASolaraqCharacterPawn; // << NEW: Forward declare character pawn
class UUserWidget;
class UWidgetComponent;

// Enum to define control modes (NEW)
UENUM(BlueprintType)
enum class EPlayerControlMode : uint8
{
    Ship        UMETA(DisplayName = "Ship Control"),
    Character   UMETA(DisplayName = "Character Control")
};

UCLASS()
class SOLARAQ_API ASolaraqPlayerController : public APlayerController, public IGenericTeamAgentInterface
{
    GENERATED_BODY()

public:
    ASolaraqPlayerController();

    // --- Generic Team Interface ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Team")
    FGenericTeamId TeamId = FGenericTeamId(0); // Player Team ID

    virtual FGenericTeamId GetGenericTeamId() const override;
    // virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override; // Implement if needed
    // --- End Generic Team Interface ---

    // --- Pawn Getters (NEW/MODIFIED) ---
    ASolaraqShipBase* GetControlledShip() const;
    ASolaraqCharacterPawn* GetControlledCharacter() const;

    void InitiateLevelTransitionToCharacter(FName TargetLevelName, FName DockingPadID = NAME_None);
    void InitiateLevelTransitionToShip(FName TargetShipLevelName);
    void ApplyInputContextForCurrentMode();
protected:
    //~ Begin APlayerController Interface
    virtual void OnPossess(APawn* InPawn) override; // << UNCOMMENTED
    virtual void OnUnPossess() override;             // << UNCOMMENTED
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnRep_Pawn() override; 
    //~ End APlayerController Interface

    // --- Input Assets ---
    // Assign these Input Action Assets in the derived Blueprint Controller (BP_SolaraqPlayerController)

    /** Default Input Mapping Context (Used for Ship Controls) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext; // This will be our Ship IMC

    /** NEW: Input Mapping Context for Character Controls */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input")
    TObjectPtr<UInputMappingContext> IMC_CharacterControls;

    // --- Ship Input Actions (Existing) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Ship")
    TObjectPtr<UInputAction> MoveAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Ship")
    TObjectPtr<UInputAction> TurnAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Ship")
    TObjectPtr<UInputAction> FireAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solaraq|Input|Ship")
    TObjectPtr<UInputAction> FireMissileAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Ship")
    TObjectPtr<UInputAction> BoostAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solaraq|Input|Ship")
    TObjectPtr<UInputAction> ToggleLockAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solaraq|Input|Ship")
    TObjectPtr<UInputAction> SwitchTargetAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Shared") // Add to Shared or Ship
    TObjectPtr<UInputAction> InteractAction; 

    // --- Character Input Actions (NEW) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    TObjectPtr<UInputAction> CharacterMoveAction;

    // --- Shared Input Actions (NEW) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Shared")
    TObjectPtr<UInputAction> TogglePawnModeAction;


    // --- Input Handling Functions (Existing Ship Handlers) ---
    void HandleMoveInput(const FInputActionValue& Value);
    void HandleTurnInput(const FInputActionValue& Value);
    void HandleTurnCompleted(const FInputActionValue& Value);
    void HandleFireMissileRequest(const FInputActionValue& Value);
    void HandleFireRequest();
    void HandleBoostStarted(const FInputActionValue& Value);
    void HandleBoostCompleted(const FInputActionValue& Value);
    void HandleToggleLock();
    void HandleSwitchTarget(const FInputActionValue& Value);
    void HandleInteractInput();

    // --- Input Handling Functions (NEW Character & Shared Handlers) ---
    void HandleCharacterMoveInput(const FInputActionValue& Value);
    void HandleTogglePawnModeInput();


    // --- Homing Lock System (Existing) ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Homing Lock")
    bool bIsHomingLockActive;
    UPROPERTY(VisibleAnywhere, Category = "Solaraq|Homing Lock")
    TArray<TWeakObjectPtr<AActor>> PotentialHomingTargets;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Homing Lock")
    int32 LockedHomingTargetIndex;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Homing Lock")
    TWeakObjectPtr<AActor> LockedHomingTargetActor;
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Homing Lock")
    float HomingTargetScanRange;
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Homing Lock")
    float HomingTargetScanConeAngleDegrees;
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Homing Lock")
    float HomingTargetScanInterval;
    FTimerHandle TimerHandle_ScanTargets;

    // --- HUD / Widgets (Existing) ---
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Homing Lock|UI")
    TSubclassOf<UUserWidget> TargetMarkerWidgetClass;
    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, TObjectPtr<UUserWidget>> TargetMarkerWidgets;
    UPROPERTY()
    TObjectPtr<UUserWidget> LockedTargetMarkerWidgetInstance; // Still seems unused based on your original CPP, consider if needed

    void UpdatePotentialTargets();
    void UpdateTargetWidgets();
    void ClearTargetWidgets();
    void SelectTargetByIndex(int32 Index);

    // --- Pawn Control Management (NEW) ---
protected:
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Pawn Control")
    TSubclassOf<ASolaraqCharacterPawn> CharacterPawnClass;

    UPROPERTY(VisibleInstanceOnly, Category = "Solaraq|Pawn Control")
    TObjectPtr<ASolaraqShipBase> PossessedShipPawn;

    UPROPERTY(VisibleInstanceOnly, Category = "Solaraq|Pawn Control")
    TObjectPtr<ASolaraqCharacterPawn> PossessedCharacterPawn;
    
    EPlayerControlMode CurrentControlMode;

    void SwitchToMode(EPlayerControlMode NewMode);
    void ClearAllInputContexts();
    

private:
    /** Cached pointer to the Enhanced Input Component */
    UPROPERTY()
    TObjectPtr<UEnhancedInputComponent> EnhancedInputComponentRef;

    // Note: ControlledShipCached is removed in favor of PossessedShipPawn and GetControlledShip() logic.
};

// Implement GetGenericTeamId if needed
inline FGenericTeamId ASolaraqPlayerController::GetGenericTeamId() const { return TeamId; }