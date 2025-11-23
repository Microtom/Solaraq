#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

// This class does not need to be modified.
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
	 * Main interaction trigger.
	 * @param InteractingPawn The pawn that is performing the interaction.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(APawn* InteractingPawn);
};