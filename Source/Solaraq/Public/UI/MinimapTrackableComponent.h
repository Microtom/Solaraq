#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Styling/SlateBrush.h" // For FSlateBrush
#include "MinimapTrackableComponent.generated.h"

// Forward declaration
class UMinimapComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class YOURPROJECTNAME_API UMinimapTrackableComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMinimapTrackableComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    // --- Configuration ---

    // The icon to display on the minimap for this actor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    FSlateBrush MinimapIcon; // Use FSlateBrush for flexibility (Texture, Material, Size)

    // Tint color for the icon.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    FLinearColor IconColor = FLinearColor::White;

    // Size of the icon on the minimap (can be overridden by FSlateBrush).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap", meta = (EditCondition = "!MinimapIcon.HasUObject", EditConditionHides))
    FVector2D IconSize = FVector2D(16.0f, 16.0f);

    // Should the icon rotate on the minimap to match the actor's Yaw rotation? (Good for player, ships)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    bool bTrackRotation = false;

    // Priority for drawing - higher values draw on top. Useful for player, objectives.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    int32 DrawPriority = 0;

    // Optional: Only show on map if within this world distance from the player. 0 means always check.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    float VisibilityRadius = 0.0f;

    // Optional: Clamp the icon to the edge of the minimap when outside the view radius, instead of hiding.
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    // bool bClampToEdge = false; // Implement later if needed

    // --- Runtime Data ---

    // Get the owning actor's location (cached potentially)
    UFUNCTION(BlueprintPure, Category = "Minimap")
    FVector GetTrackedActorLocation() const;

    // Get the owning actor's rotation (cached potentially)
    UFUNCTION(BlueprintPure, Category = "Minimap")
    FRotator GetTrackedActorRotation() const;

private:
    // Reference to the central minimap system (found via Player Controller)
    UPROPERTY()
    TWeakObjectPtr<UMinimapComponent> OwningMinimapComponent;

    // Helper to find the MinimapComponent
    void RegisterWithMinimapSystem();
    void UnregisterWithMinimapSystem();
};
