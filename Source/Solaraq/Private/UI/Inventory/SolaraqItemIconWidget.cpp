// SolaraqItemIconWidget.cpp

#include "UI/Inventory/SolaraqItemIconWidget.h" // Adjust path as needed
#include "Items/ItemDataAssetBase.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void USolaraqItemIconWidget::Initialize(const FPlacedItem& InItemInfo)
{
	// Store the item info for later use (e.g., if the player starts dragging this widget).
	this->ItemInfo = InItemInfo;
	
	// Validate that we have the necessary data and components.
	if (!ItemIcon || !QuantityText || !ItemInfo.ItemData)
	{
		return;
	}

	// --- Set the Icon ---
	// Get the icon texture from the item's data asset.
	UTexture2D* IconTexture = ItemInfo.ItemData->Icon;
	if (IconTexture)
	{
		ItemIcon->SetBrushFromTexture(IconTexture);
		ItemIcon->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		// If there's no icon, hide the image to prevent showing a default white square.
		ItemIcon->SetVisibility(ESlateVisibility::Hidden);
	}

	// --- Set the Quantity Text ---
	// Only show the quantity text if the item is stackable and the count is greater than 1.
	if (ItemInfo.ItemData->bIsStackable && ItemInfo.Quantity > 1)
	{
		QuantityText->SetText(FText::AsNumber(ItemInfo.Quantity));
		QuantityText->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		// Otherwise, hide the text block completely.
		QuantityText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// NOTE: You will later override OnDragDetected here to start a drag-and-drop operation.
// When you do, you can create the DragDropOperation and pass `this->ItemInfo` into it
// as the payload, so the grid knows which item is being dropped.