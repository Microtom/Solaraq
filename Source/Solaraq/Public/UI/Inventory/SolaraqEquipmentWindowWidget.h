// SolaraqEquipmentWindowWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/ItemDataAssetBase.h" // For EEquipmentSlot
#include "Items/InventoryComponent.h" // For FPlacedItem
#include "SolaraqEquipmentWindowWidget.generated.h"

class USolaraqInventoryGridWidget;
class USolaraqEquipmentComponent;
class UInventoryComponent;
class USolaraqCircularButton; // Forward declare Button

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSolaraqEquipmentClose);

UCLASS()
class SOLARAQ_API USolaraqEquipmentWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	// Delegate that fires when the UI requests to close itself
	UPROPERTY(BlueprintAssignable, Category = "Solaraq|Events")
	FOnSolaraqEquipmentClose OnCloseRequested;
	
protected:
	// --- Input & Dragging Overrides ---
	// Prevents click-through to character movement and handles window dragging
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	UFUNCTION()
	void RefreshWindow();

	UFUNCTION()
	void CloseWindow();

	UFUNCTION()
	void HandleEquipmentDrop(const FPlacedItem& DroppedItem, USolaraqInventoryGridWidget* SourceGrid, USolaraqInventoryGridWidget* TargetGrid, FIntPoint TargetCoord);

	// --- The Visual Grids ---
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USolaraqInventoryGridWidget> Grid_Head;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USolaraqInventoryGridWidget> Grid_Body;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USolaraqInventoryGridWidget> Grid_MainHand;
    
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USolaraqInventoryGridWidget> Grid_OffHand;

	// --- UI Controls ---
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USolaraqCircularButton> CloseButton;

private:
	UPROPERTY()
	TObjectPtr<USolaraqEquipmentComponent> EquipmentComp;
    
	UPROPERTY()
	TObjectPtr<UInventoryComponent> InventoryComp; 

	// Helper to map a visual grid to a logical slot enum
	TMap<USolaraqInventoryGridWidget*, EEquipmentSlot> GridToSlotMap;
};