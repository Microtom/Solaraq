// SolaraqInventoryWindowWidget.cpp

#include "UI/Inventory/SolaraqInventoryWindowWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/Inventory/SolaraqInventoryGridWidget.h" // Assuming this is the correct path

void USolaraqInventoryWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// We can set default values or perform initial setup here
	SetMoneyText(0); // Initialize money text
}

FReply USolaraqInventoryWindowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// We only care about the Left Mouse Button for dragging
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Is the mouse cursor currently over our designated header widget?
		// IsHovered() checks the widget passed to it (Header_Border).
		if (Header_Border && Header_Border->IsHovered())
		{
			bIsDragging = true;

			// Calculate the offset. This allows us to drag the window from any point
			// on the header, not just its top-left corner.
			// GetScreenSpacePosition() is the top-left of the widget.
			// InMouseEvent.GetScreenSpacePosition() is the current mouse position.
			DragOffset = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

			// We need to capture the mouse to ensure we keep receiving mouse move events
			// even if the cursor leaves the widget.
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}

	// If we're not dragging the header, let the event be handled by other widgets (like item icons).
	return FReply::Unhandled();
}

FReply USolaraqInventoryWindowWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// If we were dragging and the left mouse button is released, stop dragging.
	if (bIsDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDragging = false;

		// Release the mouse capture. It's important to balance every CaptureMouse with a ReleaseMouse.
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply USolaraqInventoryWindowWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// If we are currently in "drag mode"
	if (bIsDragging)
	{
		// Get the current absolute screen position of the mouse
		FVector2D MousePosition = InMouseEvent.GetScreenSpacePosition();
		
		// Subtract the initial offset to get the new top-left position for our widget
		FVector2D NewPosition = MousePosition - DragOffset;

		// Set the widget's position in the viewport.
		// The `false` for bRemoveDPIScale is usually what you want here.
		SetPositionInViewport(NewPosition, false);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}


void USolaraqInventoryWindowWidget::SetMoneyText(int32 Amount)
{
	if (MoneyText_Block)
	{
		// FText::Format is a safe and localization-friendly way to create formatted text.
		FText NewText = FText::Format(NSLOCTEXT("SolaraqInventory", "MoneyFormat", "Money: {0}G"), FText::AsNumber(Amount));
		MoneyText_Block->SetText(NewText);
	}
}