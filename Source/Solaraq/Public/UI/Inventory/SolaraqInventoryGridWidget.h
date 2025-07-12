// SolaraqInventoryGridWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/InventoryComponent.h" // We need FPlacedItem
#include "SolaraqInventoryGridWidget.generated.h"

// Forward declarations
class UCanvasPanel;
class USolaraqInventorySlotWidget;
class UInventoryComponent;
class UButton;
class UImage;
class UTextBlock;

/**
 * Main inventory widget that procedurally generates its grid of slots
 * and places item icons on top.
 */

UCLASS()
class SOLARAQ_API USolaraqInventoryGridWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Main function to rebuild the entire inventory display from scratch. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshInventory();

	/** Tells the grid to ignore a specific item during the next RefreshInventory call. */
	void SetItemToIgnore(const FGuid& ItemID);

	/** Tells the grid to stop ignoring any items. */
	void ClearIgnoredItem();

	float GetSlotPixelSize() const { return SlotPixelSize; }
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	UCanvasPanel* GetHighlightCanvas() const { return HighlightCanvas; }
	TSubclassOf<UUserWidget> GetHighlightWidgetClass() const { return HighlightWidgetClass; }

protected:
	// Called when the widget is created. We'll use it to bind to the inventory update delegate.
	virtual void NativeConstruct() override;

	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	
	// --- UPROPERTY Bindings ---
	// Your UMG widget hierarchy must have Canvas Panels with these exact names.

	// The layer for the procedurally generated background slots.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> SlotCanvas;

	// The layer for the item icons themselves.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> ItemIconCanvas;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> HighlightCanvas;
	
	// --- Blueprint-Assignable Properties ---

	// The class of our smart tile widget. Assign WBP_InventorySlot in the editor.
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Inventory")
	TSubclassOf<USolaraqInventorySlotWidget> InventorySlotClass;
	
	// The class for the item icon widget. We'll create a simple one.
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Inventory")
	TSubclassOf<UUserWidget> ItemIconClass; // We'll need to create this simple widget

	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Inventory")
	TSubclassOf<UUserWidget> HighlightWidgetClass;
	
	// The size of a single grid slot in pixels.
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Inventory")
	float SlotPixelSize = 80.f;

private:
	// A cached pointer to the inventory component for quick access.
	UPROPERTY()
	TObjectPtr<UInventoryComponent> InventoryComponent;

	// When a drag starts, we set this ID. RefreshInventory will skip drawing this item.
	FGuid ItemIDToIgnoreOnRefresh;
};