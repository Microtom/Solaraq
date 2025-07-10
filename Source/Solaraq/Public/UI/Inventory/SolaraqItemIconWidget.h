// SolaraqItemIconWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/InventoryComponent.h" // We need access to the FPlacedItem struct
#include "SolaraqItemIconWidget.generated.h"

// Forward declarations for the UMG components we will bind to.
class UImage;
class UTextBlock;
class UButton; // Including a button is great for handling clicks and drag operations.

/**
 * A widget that visually represents an item in the inventory grid.
 * It displays the item's icon and stack quantity.
 * It is also the primary widget for user interaction (clicking, dragging).
 */
UCLASS()
class SOLARAQ_API USolaraqItemIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Sets up the widget's appearance and stores item info based on a FPlacedItem struct.
	 * @param ItemInfo The struct containing all necessary data for this item icon.
	 */
	void Initialize(const FPlacedItem& InItemInfo);
	FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent);
	void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
	                          UDragDropOperation*& OutOperation);

protected:
	// --- UPROPERTY Bindings ---
	// Your WBP_ItemIcon Blueprint must have widgets with these exact names.

	// The button that captures all interaction. It should be the root or fill the entire widget.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ItemButton;

	// The image that will display the item's icon texture.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemIcon;

	// The text block for displaying the stack count.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> QuantityText;
	
private:
	// A copy of the item's info for this widget to reference, e.g., for drag-drop operations.
	UPROPERTY()
	FPlacedItem ItemInfo;
};