#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MinimapComponent.generated.h"

// Forward Declarations
class UMinimapTrackableComponent;
class UUserWidget;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class YOURPROJECTNAME_API UMinimapComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMinimapComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    // --- Configuration ---

    // The UMG Widget class to use for the minimap display.
    UPROPERTY(EditDefaultsOnly, Category = "Minimap")
    TSubclassOf<UUserWidget> MinimapWidgetClass;

    // How much world distance (in UU) corresponds to the radius of the minimap view.
    // E.g., 10000 means actors 10000 units away horizontally will be at the edge.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap", meta = (ClampMin = "100.0"))
    float MapWorldRadius = 10000.0f;

    // The Z coordinate actors are projected onto for minimap calculations.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    float ProjectionZ = 0.0f;


    // --- API for Trackables ---
    void RegisterTrackable(UMinimapTrackableComponent* Trackable);
    void UnregisterTrackable(UMinimapTrackableComponent* Trackable);

    // --- API for Widget ---
    // Called by the widget to get the data it needs to draw.
    UFUNCTION(BlueprintPure, Category = "Minimap")
    const TArray<TWeakObjectPtr<UMinimapTrackableComponent>>& GetTrackedComponents() const { return TrackedComponents; }

    UFUNCTION(BlueprintPure, Category = "Minimap")
    FVector GetPlayerPawnLocationProjected() const;

    UFUNCTION(BlueprintPure, Category = "Minimap")
    FRotator GetPlayerPawnRotation() const;

private:
    // The actual instance of the minimap widget.
    UPROPERTY(Transient) // Don't save
    TObjectPtr<UUserWidget> MinimapWidgetInstance;

    // List of components currently registered and needing display.
    // Using TWeakObjectPtr prevents issues if a tracked actor is destroyed unexpectedly.
    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<UMinimapTrackableComponent>> TrackedComponents;

    // Helper to get the owning player controller's pawn
    APawn* GetPlayerPawn() const;
};