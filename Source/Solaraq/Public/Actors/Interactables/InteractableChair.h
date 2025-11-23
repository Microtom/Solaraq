#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Systems/Interfaces/InteractableInterface.h" // Include the interface here
#include "InteractableChair.generated.h" // Must be the LAST include

UCLASS()
class SOLARAQ_API AInteractableChair : public AActor, public IInteractableInterface
{
	GENERATED_BODY()
	
public:	
	AInteractableChair();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* DefaultSceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* ChairMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* SeatAttachmentPoint;

	// Track who is currently sitting
	UPROPERTY()
	class APawn* SeatedPawn;

public:
	// --- Interface Implementation ---
	// We override the _Implementation version because it is a BlueprintNativeEvent
	virtual void Interact_Implementation(APawn* InteractingPawn) override;
};