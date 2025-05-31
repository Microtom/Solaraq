// SolaraqBasePlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "SolaraqBasePlayerController.generated.h"

// Forward Declarations
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent;
class ASolaraqShipBase; // Forward declare ship class for transition function
class USolaraqGameInstance;

UCLASS(Abstract) // This controller is a base class and should not be instantiated directly.
class SOLARAQ_API ASolaraqBasePlayerController : public APlayerController, public IGenericTeamAgentInterface
{
    GENERATED_BODY()

public:
    ASolaraqBasePlayerController();

    // --- Generic Team Interface ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Team")
    FGenericTeamId TeamId = FGenericTeamId(0); // Default to Player Team ID

    virtual FGenericTeamId GetGenericTeamId() const override;
    // ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override; // Implement if common logic for all controlled pawns is needed.
    // --- End Generic Team Interface ---

    // --- Level Transition ---
    // Initiates a transition to a character-centric level.
    // Called by pawn/UI interaction. For multiplayer, this should lead to a server request.
    virtual void RequestCharacterLevelTransition(FName TargetLevelName, FName DockingPadID = NAME_None, ASolaraqShipBase* FromShip = nullptr);

    // Initiates a transition back to a ship-centric level.
    // Called by pawn/UI interaction. For multiplayer, this should lead to a server request.
    virtual void RequestShipLevelTransition(FName TargetShipLevelName);

    // Server-side function to instruct this player controller's client to seamlessly travel.
    // This is called ON THE SERVER by game logic (e.g., after a client makes a Server RPC request to transition).
    virtual void Server_InitiateSeamlessTravelToLevel(FName TargetLevelName, bool bIsCharacterLevel, FName PlayerStartOrPadID = NAME_None, ASolaraqShipBase* FromShipForGI = nullptr);


protected:
    //~ Begin APlayerController Interface
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void SetupInputComponent() override;
    virtual void OnRep_Pawn() override; // Important for client-side pawn updates.
    virtual void Tick(float DeltaTime) override; // Used by derived classes (e.g., ShipController for target widgets).
    //~ End APlayerController Interface

    /** Helper to get the GameInstance, provides access to shared game state like transition data. */
    USolaraqGameInstance* GetSolaraqGameInstance() const;
    
    /** Cached pointer to the Enhanced Input Component, used by derived classes to bind actions. */
    UPROPERTY()
    TObjectPtr<UEnhancedInputComponent> EnhancedInputComponentRef;

    // --- Shared Input Actions ---
    // This action is typically bound in derived controllers to trigger level transitions or other interactions.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Shared")
    TObjectPtr<UInputAction> InteractAction;

public:
    // --- Enhanced Input System Utilities ---
    // Clears all mapping contexts from the subsystem. Use with caution.
    void ClearAllInputContexts(UEnhancedInputLocalPlayerSubsystem* InputSubsystem);
    // Adds a specific mapping context to the subsystem with a given priority.
    void AddInputContext(UEnhancedInputLocalPlayerSubsystem* InputSubsystem, UInputMappingContext* ContextToAdd, int32 Priority = 0);
};

// Inline implementation for GetGenericTeamId
inline FGenericTeamId ASolaraqBasePlayerController::GetGenericTeamId() const { return TeamId; }