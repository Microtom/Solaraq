// --- START OF FILE DockingPadComponent.h ---

// DockingPadComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DockingPadComponent.generated.h"

// Forward Declarations
class UBoxComponent; // Or USphereComponent
class ASolaraqShipBase;
class USceneComponent;
class UPrimitiveComponent; // Added for clarity

/**
 * @brief Represents a single docking point on an Actor (e.g., Space Station).
 *
 * This component manages the state (Available, Occupied) of a docking pad.
 * It uses a PrimitiveComponent (assigned via DockingVolume) to detect potential ships.
 * It requires DockingVolume and optionally AttachPoint to be assigned in the owning Actor's Blueprint or C++ setup.
 * It handles initiating the docking/undocking process (server-authoritative).
 */
UENUM(BlueprintType)
enum class EDockingStatus : uint8
{
	Available UMETA(DisplayName = "Available"),
	Occupied  UMETA(DisplayName = "Occupied"),
	Docking   UMETA(DisplayName = "Docking"), // Optional intermediate state
	Undocking UMETA(DisplayName = "Undocking") // Optional intermediate state
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SOLARAQ_API UDockingPadComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Constructor */
	UDockingPadComponent();

protected:
	//~ Begin UActorComponent Interface
	/** Called when the game starts or when spawned. */
	virtual void BeginPlay() override;
	//~ End UActorComponent Interface

	//~ Begin AActor Interface (for Replication)
	/** Returns properties that are replicated for the lifetime of the actor channel */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor Interface

	// --- Components (Setup in owning Actor/Blueprint) ---

	/** The volume that triggers docking procedure. Needs to be created and setup in the owning actor (e.g., BP_SpaceStation). Assign a Box or Sphere Component here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking Pad|Setup")
	TObjectPtr<UPrimitiveComponent> DockingVolume;

	/** The specific point/component on the owning station where the ship should visually attach. If null, attaches to owner's root. Needs setup in owning actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking Pad|Setup")
	TObjectPtr<USceneComponent> AttachPoint;

	// --- Docking Logic ---

	/** Automatically docks any valid ship that enters the volume (if Available). If false, requires external trigger (e.g., player interaction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking Pad|Behavior")
	bool bAutoDockOnEnter = true;

	/** Current status of the docking pad (Server authoritative). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Docking Pad|State", ReplicatedUsing = OnRep_DockingStatus)
	EDockingStatus CurrentDockingStatus = EDockingStatus::Available;

	/** Reference to the ship currently docked or docking/undocking at this pad (Server authoritative). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Docking Pad|State", ReplicatedUsing = OnRep_DockedShip)
	TObjectPtr<ASolaraqShipBase> CurrentDockedShip;

	// --- Overlap Handling (Server-Side) ---

	/** Called when something enters the DockingVolume (Server-side binding). */
	UFUNCTION()
	void OnDockingVolumeOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Called when something leaves the DockingVolume (Server-side binding). */
	UFUNCTION()
	void OnDockingVolumeOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// --- Replication ---

	/** Replication notification function for CurrentDockingStatus. Called on Clients. */
	UFUNCTION()
	void OnRep_DockingStatus();

	/** Replication notification function for CurrentDockedShip. Called on Clients. */
	UFUNCTION()
	void OnRep_DockedShip();

public:
	// --- Public Control Functions (Server-Side Logic) ---

	/**
	 * Attempts to initiate docking for the specified ship.
	 * Checks if pad is available and ship is valid. Called on Server.
	 * @param ShipToDock The ship attempting to dock.
	 * @return True if docking was successfully initiated, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Docking Pad")
	bool InitiateDocking(ASolaraqShipBase* ShipToDock);

	/**
	 * Attempts to initiate undocking for the currently docked ship.
	 * Checks if a ship is currently docked. Called on Server.
	 * @return True if undocking was successfully initiated, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Docking Pad")
	bool InitiateUndocking();

	// --- Getters ---

	/** Gets the current docking status. */
	UFUNCTION(BlueprintPure, Category = "Docking Pad")
	EDockingStatus GetDockingStatus() const { return CurrentDockingStatus; }

	/** Gets the ship currently docked or transitioning at this pad. */
	UFUNCTION(BlueprintPure, Category = "Docking Pad")
	ASolaraqShipBase* GetCurrentDockedShip() const { return CurrentDockedShip; }

	/** Gets the designated attach point component. May be null if not set. */
	 UFUNCTION(BlueprintPure, Category = "Docking Pad")
	USceneComponent* GetAttachPoint() const { return AttachPoint; }
};

// --- END OF FILE DockingPadComponent.h ---