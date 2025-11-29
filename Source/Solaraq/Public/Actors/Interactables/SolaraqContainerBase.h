#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/InventoryComponent.h"
#include "SolaraqContainerBase.generated.h"

UCLASS()
class SOLARAQ_API ASolaraqContainerBase : public AActor
{
	GENERATED_BODY()
	
public:	
	ASolaraqContainerBase();

	// Called by Controller when the player arrives at the container
	virtual void OpenContainer(APlayerController* PlayerController);
	
	// Called when the UI is closed
	virtual void CloseContainer();

	// Helper for navigation
	FVector GetInteractionLocation() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container")
	TObjectPtr<UInventoryComponent> InventoryComponent;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// Optional: Separate lid mesh for animation
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container")
	TObjectPtr<UStaticMeshComponent> LidMeshComponent;

	// Basic animation state
	UPROPERTY(EditAnywhere, Category = "Container")
	FRotator OpenLidRotation = FRotator(0, 0, 110);

	FRotator ClosedLidRotation;
	bool bIsOpen = false;
};