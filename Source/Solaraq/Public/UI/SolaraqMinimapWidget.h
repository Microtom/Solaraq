#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SolaraqMinimapInterface.h" // Include the interface
#include "SolaraqMinimapWidget.generated.h"

class UCanvasPanel;
class UImage;

/** Helper struct to track active icons so we don't recreate them every frame */
struct FMinimapIconData
{
	TWeakObjectPtr<AActor> OwnerActor;
	UUserWidget* IconWidget;
};

UCLASS()
class SOLARAQ_API USolaraqMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	// The Canvas Panel inside the WBP that will hold the icons
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> MapContainer;

	// The background image (the grid/circle)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> BackgroundImage;

	// The Widget Blueprint class to spawn for each blip (dot/icon)
	UPROPERTY(EditAnywhere, Category = "Minimap Config")
	TSubclassOf<UUserWidget> IconTemplateClass;

	// Total width of the game world in Unreal Units (e.g., 200,000 for a large sector)
	UPROPERTY(EditAnywhere, Category = "Minimap Config")
	float MapWorldSize = 100000.0f;

	// How far (in Unreal Units) the radar sees.
	// Objects further than this will be clamped to the edge or hidden.
	UPROPERTY(EditAnywhere, Category = "Minimap Config")
	float RadarRange = 25000.0f;
private:
	TArray<FMinimapIconData> ActiveIcons;
	float UpdateTimer = 0.0f;

	// Helper to spawn/destroy icons
	void RefreshTrackedActors();
	
	// Math helper: World (3D) -> Widget (2D)
	FVector2D WorldToMapCoords(FVector WorldLoc, FVector2D WidgetSize);
};