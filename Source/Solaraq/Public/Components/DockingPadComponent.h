// Components/DockingPadComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "DockingPadComponent.generated.h"

class UBoxComponent;
class ASolaraqShipBase;

// Enum to manage the state of docking, useful for transitions and UI.
UENUM(BlueprintType)
enum class EDockingStatus : uint8
{
	None UMETA(DisplayName = "Not Docked"),
	AttemptingDock UMETA(DisplayName = "Attempting Dock"), // Ship is trying to dock
	Docking UMETA(DisplayName = "Docking In Progress"),  // Locking sequence
	Docked UMETA(DisplayName = "Docked"),
	Undocking UMETA(DisplayName = "Undocking In Progress") // Unlocking sequence
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SOLARAQ_API UDockingPadComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UDockingPadComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Solaraq|Docking")
	FName TargetCharacterLevelName; // Level to load when this pad is used for character entry

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solaraq|Docking")
	FName DockingPadUniqueID; 

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The volume that detects ships for docking. Made visible for gameplay. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|DockingPad")
	TObjectPtr<UBoxComponent> DockingTriggerVolume;

	/** Server-side: The ship currently docked or attempting to dock with this pad. */
	UPROPERTY(Transient) // Server-side cache, not replicated directly by pad
	TObjectPtr<ASolaraqShipBase> OccupyingShip_Server;

public:
	/** Called when something begins overlapping the DockingTriggerVolume. */
	UFUNCTION()
	void OnDockingVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Called when something stops overlapping the DockingTriggerVolume. */
	UFUNCTION()
	void OnDockingVolumeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/**
	 * Server-side function to mark this pad as being occupied by a specific ship.
	 * Called by the ship when it successfully initiates docking.
	 * @param Ship The ship that is now occupying this pad.
	 */
	void SetOccupyingShip_Server(ASolaraqShipBase* Ship);

	/**
	 * Server-side function to mark this pad as free.
	 * Called by the ship when it successfully undocks.
	 */
	void ClearOccupyingShip_Server();

	/**
	 * Checks if the pad is currently considered free on the server.
	 * @return True if no ship is currently docked or in the process of docking with this pad.
	 */
	bool IsPadFree_Server() const;

	/** Returns the attach point for the ship (this component itself). */
	USceneComponent* GetAttachPoint() const { return (USceneComponent*)this; }


	// --- Later features ---
	// UPROPERTY(EditDefaultsOnly, Category = "DockingPad")
	// FVector ShipAttachRelativeOffset;

	// UPROPERTY(EditDefaultsOnly, Category = "DockingPad")
	// FRotator ShipAttachRelativeRotation;
};