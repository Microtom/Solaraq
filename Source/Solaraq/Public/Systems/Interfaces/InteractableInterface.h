// InteractableInterface.h

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class SOLARAQ_API IInteractableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Main interaction trigger. Called when the player wants to start interacting.
	 * The object itself can decide what to do (e.g., open a door, start a cutscene).
	 * For a chair, this will likely be empty, as the Pawn drives the logic.
	 * @param InteractingPawn The pawn that is performing the interaction.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(APawn* InteractingPawn);

	/**
	 * Gets the world-space transform where the character should be to *begin* the interaction.
	 * For a chair, this is the spot in front of it.
	 * @param OutTransform The resulting approach transform.
	 * @return True if this object provides an approach point, false otherwise.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool GetInteractionApproachPoint(FTransform& OutTransform);

	/**
	 * Gets the world-space transform for the final, completed interaction state.
	 * For a chair, this is the seated position.
	 * @param OutTransform The resulting final transform.
	 * @return True if this object provides a final interaction point, false otherwise.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool GetFinalInteractionTransform(FTransform& OutTransform);
};