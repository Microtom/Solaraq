
// SolaraqHUDWidget.cpp
#include "UI/SolaraqHUDWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/SolaraqWidgetDragOperation.h"
#include "Logging/SolaraqLogChannels.h"
#include "UI/Inventory/SolaraqItemDragOperation.h"
#include "Pawns/SolaraqCharacterPawn.h"
#include "Items/InventoryComponent.h"
#include "UI/Inventory/SolaraqInventoryGridWidget.h"
UCanvasPanel* USolaraqHUDWidget::GetMainCanvas()
{
return MainCanvas;
}
bool USolaraqHUDWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

// --- 1. Check if a UI WIDGET is being dropped for repositioning ---
if (USolaraqWidgetDragOperation* WidgetDragOp = Cast<USolaraqWidgetDragOperation>(InOperation))
{		
	if (!WidgetDragOp->WidgetReference) return false;
	
	FVector2D MouseAbsPos = InDragDropEvent.GetScreenSpacePosition();
	FVector2D NewWidgetAbsPos = MouseAbsPos - WidgetDragOp->DragOffset;
	FVector2D FinalLocalPos = InGeometry.AbsoluteToLocal(NewWidgetAbsPos);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(WidgetDragOp->WidgetReference->Slot))
	{
		// --- DEBUG LOGGING ---
		UE_LOG(LogTemp, Warning, TEXT("HUD DROP | MouseAbs: %s | Offset: %s | TargetAbs: %s | FinalLocal: %s"), 
			*MouseAbsPos.ToString(), 
			*WidgetDragOp->DragOffset.ToString(), 
			*NewWidgetAbsPos.ToString(), 
			*FinalLocalPos.ToString());

		UE_LOG(LogTemp, Warning, TEXT("HUD DROP | Old Anchors: Min %s Max %s | Old Alignment: %s"), 
			*CanvasSlot->GetAnchors().Minimum.ToString(),
			*CanvasSlot->GetAnchors().Maximum.ToString(),
			*CanvasSlot->GetAlignment().ToString());
		// ---------------------

		// --- FIX: FORCE ANCHORS TO TOP-LEFT ---
		// If Anchors are centered (0.5,0.5), SetPosition acts as an offset from the center of the screen.
		// By forcing them to (0,0), SetPosition acts as absolute pixel coordinates from top-left.
		FAnchors TopLeftAnchors(0.f, 0.f, 0.f, 0.f);
		CanvasSlot->SetAnchors(TopLeftAnchors);
		CanvasSlot->SetAlignment(FVector2D(0.f, 0.f)); // Ensure pivot is top-left
		
		CanvasSlot->SetPosition(FinalLocalPos);
		
		// If the window size gets messed up by changing anchors (happens if it was stretched), reset size here if needed:
		// CanvasSlot->SetSize(DesiredSize); 

		WidgetDragOp->WidgetReference->SetVisibility(ESlateVisibility::Visible);
		return true;
	}
	return false;
}

// --- 2. If not a UI widget, check if an INVENTORY ITEM is being dropped ---
if (USolaraqItemDragOperation* ItemDragOp = Cast<USolaraqItemDragOperation>(InOperation))
{
	if (!ItemDragOp->ItemInfo.IsValid() || !ItemDragOp->SourceGrid) return false;
	
	UInventoryComponent* SourceInventory = ItemDragOp->SourceGrid->GetContextInventory();
	if (!SourceInventory) return false;
	
	const FPlacedItem ItemToDrop = ItemDragOp->ItemInfo;

	SourceInventory->RemoveItem(ItemToDrop.ItemID, ItemToDrop.Quantity);

	if (ASolaraqCharacterPawn* PlayerPawn = Cast<ASolaraqCharacterPawn>(GetOwningPlayerPawn()))
	{
		PlayerPawn->DropItem(ItemToDrop.ItemData, ItemToDrop.Quantity);
		return true;
	}
	
	return false;
}

return false;
}