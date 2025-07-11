// SolaraqItemDragOperation.cpp

#include "UI/Inventory/SolaraqItemDragOperation.h"
#include "UI/Inventory/SolaraqInventoryGridWidget.h"
#include "UI/Inventory/SolaraqItemIconWidget.h"

void USolaraqItemDragOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);

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