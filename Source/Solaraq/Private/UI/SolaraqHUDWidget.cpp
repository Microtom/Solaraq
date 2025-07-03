// SolaraqHUDWidget.cpp

#include "UI/SolaraqHUDWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/SolaraqWidgetDragOperation.h"
#include "Logging/SolaraqLogChannels.h"

UCanvasPanel* USolaraqHUDWidget::GetMainCanvas()
{
	return MainCanvas;
}

bool USolaraqHUDWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	UE_LOG(LogTemp, Log, TEXT("--- HUD::NativeOnDrop CALLED --- If you see this, the HUD is correctly hit-testable."));

	// Try to cast the operation to our custom class.
	USolaraqWidgetDragOperation* DragOperation = Cast<USolaraqWidgetDragOperation>(InOperation);
	if (!DragOperation)
	{
		UE_LOG(LogTemp, Warning, TEXT("  > Drop failed: Operation was not a USolaraqWidgetDragOperation."));
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("  > Cast to USolaraqWidgetDragOperation successful."));
	
	if (!DragOperation->WidgetReference)
	{
		UE_LOG(LogTemp, Warning, TEXT("  > Drop failed: WidgetReference in operation was NULL."));
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("  > WidgetReference is valid ('%s')."), *DragOperation->WidgetReference->GetName());

	// Calculate the new position for the widget in the HUD's canvas.
	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());
	const FVector2D FinalPosition = LocalPosition - DragOperation->DragOffset;
	UE_LOG(LogTemp, Log, TEXT("  > Calculated final position: %s"), *FinalPosition.ToString());

	// Get the canvas slot of the original widget and set its new position.
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(DragOperation->WidgetReference->Slot))
	{
		CanvasSlot->SetPosition(FinalPosition);
		UE_LOG(LogTemp, Log, TEXT("  > Successfully set position on the CanvasPanelSlot."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  > FAILED to get CanvasPanelSlot from the WidgetReference. Is it a direct child of the canvas?"));
	}

	// IMPORTANT: Restore the original widget's visibility so it can be interacted with again.
	DragOperation->WidgetReference->SetVisibility(ESlateVisibility::Visible);
	UE_LOG(LogTemp, Log, TEXT("  > Restored original widget visibility to Visible."));

	// We have successfully handled the drop.
	UE_LOG(LogTemp, Log, TEXT("  > Drop handled successfully. Returning true."));
	return true;
}