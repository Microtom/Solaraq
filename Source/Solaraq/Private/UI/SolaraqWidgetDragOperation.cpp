// SolaraqWidgetDragOperation.cpp
#include "UI/SolaraqWidgetDragOperation.h"
#include "Blueprint/UserWidget.h"

void USolaraqWidgetDragOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);

	// If the drag failed (didn't land on HUD), make sure the original window is visible again.
	if (WidgetReference)
	{
		WidgetReference->SetVisibility(ESlateVisibility::Visible);
	}
}

void USolaraqWidgetDragOperation::Drop_Implementation(const FPointerEvent& PointerEvent)
{
	Super::Drop_Implementation(PointerEvent);
    
	// The HUD handles the positioning and visibility on success, 
	// so we don't strictly need to do anything here, but it's good practice.
}