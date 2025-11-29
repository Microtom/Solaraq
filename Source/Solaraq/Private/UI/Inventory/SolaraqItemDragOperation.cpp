// SolaraqItemDragOperation.cpp

#include "UI/Inventory/SolaraqItemDragOperation.h"
#include "Components/CanvasPanel.h"
#include "Items/InventoryComponent.h"
#include "Components/CanvasPanelSlot.h"
#include "Items/ItemDataAssetBase.h"
#include "UI/Inventory/SolaraqInventoryGridWidget.h"
#include "UI/Inventory/SolaraqItemIconWidget.h"

void USolaraqItemDragOperation::Dragged_Implementation(const FPointerEvent& PointerEvent)
{
	Super::Dragged_Implementation(PointerEvent);

	if (!SourceGrid || !ItemInfo.IsValid())
	{
		return;
	}

	// FIX: Use the new GetContextInventory() function
	UInventoryComponent* Inventory = SourceGrid->GetContextInventory(); 

	// If the grid doesn't have a context (e.g. Equipment Slot), we skip the highligher
	// or you can add custom logic here for equipment highlighting.
	if (!Inventory) 
	{
		if(HighlightWidget) HighlightWidget->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
    
	// --- Calculate Target Location relative to the Source Grid ---
	const FVector2D LocalDropPosition = SourceGrid->GetCachedGeometry().AbsoluteToLocal(PointerEvent.GetScreenSpacePosition());
	const FVector2D ItemTopLeftPosition = LocalDropPosition - DragOffset;
	const int32 TargetX = FMath::RoundToInt(ItemTopLeftPosition.X / SourceGrid->GetSlotPixelSize());
	const int32 TargetY = FMath::RoundToInt(ItemTopLeftPosition.Y / SourceGrid->GetSlotPixelSize());
	const FIntPoint TargetTopLeft(TargetX, TargetY);

	// --- Check if the move would be valid ---
	const bool bCanDrop = Inventory->CanMoveItemTo(ItemInfo.ItemID, TargetTopLeft);

	if (bCanDrop)
	{
		if (!HighlightWidget) 
		{
			if (HighlightWidgetClass && SourceGrid->GetHighlightCanvas())
			{
				HighlightWidget = CreateWidget<UUserWidget>(SourceGrid->GetOwningPlayer(), HighlightWidgetClass);
				SourceGrid->GetHighlightCanvas()->AddChild(HighlightWidget);
			}
		}

		if(HighlightWidget) 
		{
			UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(HighlightWidget->Slot);
			if (CanvasSlot)
			{
				const float SlotSize = SourceGrid->GetSlotPixelSize();
				CanvasSlot->SetPosition(FVector2D(TargetTopLeft.X * SlotSize, TargetTopLeft.Y * SlotSize));
				CanvasSlot->SetSize(FVector2D(ItemInfo.ItemData->Dimensions.X * SlotSize, ItemInfo.ItemData->Dimensions.Y * SlotSize));
				HighlightWidget->SetVisibility(ESlateVisibility::Visible);
			}
		}
	}
	else 
	{
		if(HighlightWidget)
		{
			HighlightWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void USolaraqItemDragOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);

	if (HighlightWidget)
	{
		HighlightWidget->RemoveFromParent();
		HighlightWidget = nullptr;
	}
	
	if (SourceGrid)
	{
		// SourceGrid knows how to redraw itself using its cached items
		SourceGrid->ClearIgnoredItem();
		SourceGrid->Redraw();
	}
}

void USolaraqItemDragOperation::Drop_Implementation(const FPointerEvent& PointerEvent)
{
	Super::Drop_Implementation(PointerEvent);

	if (HighlightWidget)
	{
		HighlightWidget->RemoveFromParent();
		HighlightWidget = nullptr;
	}

	// Ensure the source grid stops hiding the original item icon.
	// If the item moved, the backend update will handle removing it.
	// If the item didn't move, this makes it reappear.
	if (SourceGrid)
	{
		SourceGrid->ClearIgnoredItem();
		// Optional: We can Redraw here, though usually the backend update triggers a redraw anyway.
		// It's safe to call it to ensure visual consistency immediately.
		SourceGrid->Redraw(); 
	}
}