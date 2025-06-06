// SolaraqShipPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "Controllers/SolaraqBasePlayerController.h" // Inherit from our new base
#include "SolaraqShipPlayerController.generated.h"

struct FInputActionValue;
// Forward Declarations (already in base, but good practice for clarity if used directly here)
class UInputMappingContext;
class UInputAction;
class ASolaraqShipBase;
class UUserWidget; // For target markers

UCLASS()
class SOLARAQ_API ASolaraqShipPlayerController : public ASolaraqBasePlayerController
{
    GENERATED_BODY()

public:
    ASolaraqShipPlayerController();

    void RequestTransitionToCharacterLevel(FName TargetLevelName, FName DockingPadID);
    
    // --- Pawn Getter ---
    ASolaraqShipBase* GetControlledShip() const;
    
    // Server RPC called by the client-side RequestTransitionToCharacterLevel
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ExecuteTransitionToCharacterLevel(FName TargetLevelName, FName DockingPadID);


protected:
    //~ Begin ASolaraqBasePlayerController Interface (Overrides)
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void SetupInputComponent() override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnRep_Pawn() override;
    //~ End ASolaraqBasePlayerController Interface

    // --- Input Assets ---
    /** Input Mapping Context for Ship Controls */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Ship")
    TObjectPtr<UInputMappingContext> IMC_ShipControls;

    // --- Ship Input Actions ---
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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Ship")
    TObjectPtr<UInputAction> ToggleShieldAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Ship|Mining") 
    TObjectPtr<UInputAction> FireMiningLaserAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Ship|Mining")
    TObjectPtr<UInputAction> AimLaserAction;
    
    // InteractAction is in Base, will be bound here.

    // --- Input Handling Functions (Ship Handlers) ---
    void HandleMoveInput(const FInputActionValue& Value);
    void HandleTurnInput(const FInputActionValue& Value);
    void HandleTurnCompleted(const FInputActionValue& Value);
    void HandleFireMissileRequest(const FInputActionValue& Value);
    void HandleFireRequest();
    void HandleBoostStarted(const FInputActionValue& Value);
    void HandleBoostCompleted(const FInputActionValue& Value);
    void HandleToggleLock();
    void HandleSwitchTarget(const FInputActionValue& Value);
    void HandleShipInteractInput(); // Specific handler for ship interaction
    void HandleToggleShieldInput(); // Handler for shield toggle
    void HandleFireMiningLaserStarted(const FInputActionValue& Value); 
    void HandleFireMiningLaserCompleted(const FInputActionValue& Value);
    void HandleAimLaserTriggered(const FInputActionValue& Value);
    void HandleAimLaserCompleted(const FInputActionValue& Value);
    
private:
    // --- Homing Lock System ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Homing Lock", meta=(AllowPrivateAccess="true"))
    bool bIsHomingLockActive;

    UPROPERTY(VisibleAnywhere, Category = "Solaraq|Homing Lock", meta=(AllowPrivateAccess="true"))
    TArray<TWeakObjectPtr<AActor>> PotentialHomingTargets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Homing Lock", meta=(AllowPrivateAccess="true"))
    int32 LockedHomingTargetIndex;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Homing Lock", meta=(AllowPrivateAccess="true"))
    TWeakObjectPtr<AActor> LockedHomingTargetActor;

    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Homing Lock")
    float HomingTargetScanRange;

    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Homing Lock")
    float HomingTargetScanConeAngleDegrees;

    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Homing Lock")
    float HomingTargetScanInterval;

    FTimerHandle TimerHandle_ScanTargets;

    // --- HUD / Widgets ---
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|UI|Homing Lock")
    TSubclassOf<UUserWidget> TargetMarkerWidgetClass;

    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, TObjectPtr<UUserWidget>> TargetMarkerWidgets;

    /** Widget class to use for the mining laser aiming indicator. Should implement IMiningAimWidgetInterface. */
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|UI|Mining")
    TSubclassOf<UUserWidget> MiningAimIndicatorWidgetClass;
    
    /** Runtime instance of the mining laser aiming indicator widget. */
    UPROPERTY(Transient) // Transient as it's managed at runtime
    TObjectPtr<UUserWidget> ActiveMiningAimIndicatorWidget;

    // --- Mining Laser Aiming State ---
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|MiningLaserAim")
    float LaserRelativeAimRateDegreesPerSecond = 60.0f; // How fast the desired aim angle changes with mouse input
    
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|MiningLaserAim")
    float MaxLaserRelativeYawDegrees = 120.0f; // Max angle from ship's forward for the desired aim
    
    // Transient state for aiming
    float CurrentLaserRelativeAimYaw = 0.0f;
    FVector2D LastAimLaserInputValue = FVector2D::ZeroVector; // Stores current mouse input for aiming

    
    void UpdatePotentialTargets();
    void UpdateTargetWidgets();
    void ClearTargetWidgets();
    void SelectTargetByIndex(int32 Index);

    void ApplyShipInputMappingContext();
};

