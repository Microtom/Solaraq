// SolaraqItemDragOperation.cpp

#include "UI/Inventory/SolaraqItemDragOperation.h"
#include "UI/Inventory/SolaraqItemIconWidget.h"

void USolaraqItemDragOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);

	// If the drag was cancelled, we must restore the visibility of the original widget
	// so the user can see it again in its original slot.
	if (OriginalWidget)
	{
		OriginalWidget->SetVisibility(ESlateVisibility::Visible);
	}
}