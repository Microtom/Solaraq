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
    UInventoryComponent* Inventory = SourceGrid->GetInventoryComponent(); // Assumes a getter exists or you make the member protected/public
    if (!Inventory) return;
    
    // --- Calculate Target Location (same logic as OnDrop) ---
	const FVector2D LocalDropPosition = SourceGrid->GetCachedGeometry().AbsoluteToLocal(PointerEvent.GetScreenSpacePosition());
	const FVector2D ItemTopLeftPosition = LocalDropPosition - DragOffset;
	const int32 TargetX = FMath::RoundToInt(ItemTopLeftPosition.X / SourceGrid->GetSlotPixelSize());
	const int32 TargetY = FMath::RoundToInt(ItemTopLeftPosition.Y / SourceGrid->GetSlotPixelSize());
	const FIntPoint TargetTopLeft(TargetX, TargetY);

    // --- Check if the move would be valid ---
    // NOTE: We need a function in InventoryComponent to check this without actually moving. Let's assume we add it in the next step.
    const bool bCanDrop = Inventory->CanMoveItemTo(ItemInfo.ItemID, TargetTopLeft);

    if (bCanDrop)
    {
        if (!HighlightWidget) // Create the highlight widget if it doesn't exist
        {
            if (HighlightWidgetClass && SourceGrid->GetHighlightCanvas())
            {
                HighlightWidget = CreateWidget<UUserWidget>(SourceGrid->GetOwningPlayer(), HighlightWidgetClass);
                SourceGrid->GetHighlightCanvas()->AddChild(HighlightWidget);
            }
        }

        if(HighlightWidget) // Position and size the widget
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
    else // The location is invalid, hide the highlight
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

	// Clean up the highlight widget when the drag ends
	if (HighlightWidget)
	{
		HighlightWidget->RemoveFromParent();
		HighlightWidget = nullptr;
	}
	
	// If the drag was cancelled, we must tell the source grid to redraw itself.
	// This will make the item reappear in its original slot.
	if (SourceGrid)
	{
		UE_LOG(LogTemp, Log, TEXT("Drag Cancelled. Refreshing source grid."));
		// Tell the grid to stop ignoring the item and then refresh.
		SourceGrid->ClearIgnoredItem();
		SourceGrid->RefreshInventory();
	}

	// The old logic of setting OriginalWidget visibility is no longer valid,
	// because RefreshInventory() destroyed the original widget.
}

void USolaraqItemDragOperation::Drop_Implementation(const FPointerEvent& PointerEvent)
{
	Super::Drop_Implementation(PointerEvent);

	// Clean up the highlight widget when the drag ends
	if (HighlightWidget)
	{
		HighlightWidget->RemoveFromParent();
		HighlightWidget = nullptr;
	}
}
