#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Systems/Interfaces/InteractableInterface.h"
#include "SolaraqInteractableChair.generated.h"

UCLASS()
class SOLARAQ_API ASolaraqInteractableChair : public AActor, public IInteractableInterface
{
	GENERATED_BODY()
	
public:	
	ASolaraqInteractableChair();

	// Getter for the location the character should walk to before sitting
	FVector GetEntryPointLocation() const;

	// The function called when the character has arrived and is ready to sit
	void Sit(class ASolaraqCharacterPawn* PawnToSit);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* DefaultSceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* ChairMesh;

	// Where the butt goes
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* SeatAttachmentPoint;

	// Where the character stands before sitting (e.g., in front of the chair)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* EntryPoint;

	UPROPERTY()
	class APawn* SeatedPawn;

public:
	// -- Interface Implementation --
    // We use _Implementation because the Interface defined it as BlueprintNativeEvent
    virtual void Interact_Implementation(APawn* InteractingPawn) override;
};