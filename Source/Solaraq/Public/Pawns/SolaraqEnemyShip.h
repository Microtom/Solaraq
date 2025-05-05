// SolaraqEnemyShip.h

#pragma once

#include "CoreMinimal.h"
#include "Pawns/SolaraqShipBase.h" // Include the base class header
#include "SolaraqEnemyShip.generated.h"

/**
 * A specialized ship pawn class for AI-controlled enemies.
 * Inherits core functionality from SolaraqShipBase and adds
 * specific methods for AI control.
 */
UCLASS()
class SOLARAQ_API ASolaraqEnemyShip : public ASolaraqShipBase
{
	GENERATED_BODY()

public:
	ASolaraqEnemyShip(); // Constructor

	/**
	 * Instructs the ship to turn towards a specific world location.
	 * Called by the AI Controller.
	 * @param TargetLocation The world location to face.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Control")
	virtual void TurnTowards(const FVector& TargetLocation);

	/**
	 * Instructs the ship to fire its primary weapon(s).
	 * Called by the AI Controller.
	 * TODO: Add parameters for specific weapon groups if needed.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Control")
	virtual void FireWeapon();

	/**
	 * Instructs the ship to apply forward/backward thrust.
	 * Could potentially override base class logic if AI needs different thrust control.
	 * For now, we can just call the base class server RPC.
	 * @param Value Input axis value (-1.0 to 1.0).
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Control")
	virtual void RequestMoveForward(float Value);


protected:
	// Add any AI-specific components or variables here if needed
	// Example: Maybe different weapon setups loaded based on AI type?

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	//~ End AActor Interface

	// --- Weapon Properties ---
	
	/** How far forward from the ship's center the projectile should spawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float MuzzleOffset = 150.0f;

private:
	// Add internal helper variables/functions if needed
	// Example: Track cooldown for firing weapons
	float LastFireTime = -1.0f;
	float FireRate = 0.5f; // Time between shots

};