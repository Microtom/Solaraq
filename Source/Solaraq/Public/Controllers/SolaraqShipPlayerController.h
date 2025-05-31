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
class UWidgetComponent; // If used

UCLASS()
class SOLARAQ_API ASolaraqShipPlayerController : public ASolaraqBasePlayerController
{
    GENERATED_BODY()

public:
    ASolaraqShipPlayerController();

    void RequestTransitionToCharacterLevel(FName TargetLevelName, FName DockingPadID, ASolaraqShipBase* FromShip);
    
    // --- Pawn Getter ---
    ASolaraqShipBase* GetControlledShip() const;

    // Server RPC called by the client-side RequestTransitionToCharacterLevel
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ExecuteTransitionToCharacterLevel(FName TargetLevelName, FName DockingPadID, ASolaraqShipBase* FromShip);


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
    UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Homing Lock|UI")
    TSubclassOf<UUserWidget> TargetMarkerWidgetClass;

    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, TObjectPtr<UUserWidget>> TargetMarkerWidgets;

    // No longer need specific PossessedShipPawn, GetControlledShip() will cast GetPawn()

    void UpdatePotentialTargets();
    void UpdateTargetWidgets();
    void ClearTargetWidgets();
    void SelectTargetByIndex(int32 Index);

    void ApplyShipInputMappingContext();
};