// SolaraqCharacterPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "Controllers/SolaraqBasePlayerController.h" // Inherit from our new base
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
    
    // Add other character actions here (e.g., Jump, Look, Crouch)
    // UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    // TObjectPtr<UInputAction> CharacterLookAction;

    // UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Input|Character")
    // TObjectPtr<UInputAction> CharacterJumpAction;

    // InteractAction is in Base, will be bound here.

    // --- Input Handling Functions (Character & Shared Handlers) ---
    void HandleCharacterMoveInput(const FInputActionValue& Value);
    // void HandleCharacterLookInput(const FInputActionValue& Value);
    // void HandleCharacterJumpInput();
    void HandleCharacterInteractInput(); // Specific handler for character interaction
    void HandlePrimaryUseStarted();
    void HandlePrimaryUseCompleted(); // For 'Release' triggers
    void HandleSecondaryUseStarted();
    void HandleSecondaryUseCompleted();

private:
    // No longer need specific PossessedCharacterPawn, GetControlledCharacter() will cast GetPawn()
    void ApplyCharacterInputMappingContext();
};