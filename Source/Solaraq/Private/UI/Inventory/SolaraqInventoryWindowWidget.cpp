// SolaraqInventoryWindowWidget.cpp

#include "UI/Inventory/SolaraqInventoryWindowWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Controllers/SolaraqCharacterPlayerController.h"
#include "UI/Inventory/SolaraqInventoryGridWidget.h"
#include "Logging/SolaraqLogChannels.h"

void USolaraqInventoryWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetMoneyText(0); 
}

FReply USolaraqInventoryWindowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (Header_Border && Header_Border->IsHovered())
		{
			bIsDragging = true;
			
			// Store the initial click position in the widget's local space.
			// This is our anchor point for the drag.
			DragOffset = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
			
			UE_LOG(LogTemp, Log, TEXT("OnMouseButtonDown: Drag started. Offset within widget: %s."), *DragOffset.ToString());
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}
	return FReply::Unhandled();
}

FReply USolaraqInventoryWindowWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDragging)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			// Get the current mouse position in the local space of this widget.
			const FVector2D CurrentMousePosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

			// The amount we need to move the widget is the difference between the current
			// mouse position and the initial click position (the offset).
			const FVector2D TranslationDelta = CurrentMousePosition - DragOffset;

			// Get the current position of the widget in the canvas.
			const FVector2D CurrentPosition = CanvasSlot->GetPosition();
			
			// The new position is the current position plus the translation delta.
			const FVector2D NewPosition = CurrentPosition + TranslationDelta;
			
			CanvasSlot->SetPosition(NewPosition);

			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

FReply USolaraqInventoryWindowWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (bIsDragging)
		{
			bIsDragging = false;
			UE_LOG(LogTemp, Log, TEXT("OnMouseButtonUp: Drag finished. Releasing mouse capture."));
			return FReply::Handled().ReleaseMouseCapture();
		}
	}
	return FReply::Unhandled();
}

// --- Unchanged Functions ---

bool USolaraqInventoryWindowWidget::NativeSupportsKeyboardFocus() const
{
	return true;
}


void USolaraqInventoryWindowWidget::SetMoneyText(int32 Amount)
{
	if (MoneyText_Block)
	{
		FText NewText = FText::Format(NSLOCTEXT("SolaraqInventory", "MoneyFormat", "Money: {0}G"), FText::AsNumber(Amount));
		MoneyText_Block->SetText(NewText);
	}
}
