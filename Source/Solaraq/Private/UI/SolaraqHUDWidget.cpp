// SolaraqHUDWidget.cpp

#include "UI/SolaraqHUDWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/SolaraqWidgetDragOperation.h"
#include "Logging/SolaraqLogChannels.h"
#include "UI/Inventory/SolaraqItemDragOperation.h" // <-- ADD THIS
#include "Pawns/SolaraqCharacterPawn.h"            // <-- ADD THIS
#include "Items/InventoryComponent.h"             // <-- ADD THIS
#include "UI/Inventory/SolaraqInventoryGridWidget.h"

UCanvasPanel* USolaraqHUDWidget::GetMainCanvas()
{
	return MainCanvas;
}

bool USolaraqHUDWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

	UE_LOG(LogTemp, Log, TEXT("--- HUD::NativeOnDrop CALLED --- If you see this, the HUD is correctly hit-testable."));
	
	// --- 1. Check if a UI WIDGET is being dropped for repositioning ---
	if (USolaraqWidgetDragOperation* WidgetDragOp = Cast<USolaraqWidgetDragOperation>(InOperation))
	{		
		if (!WidgetDragOp->WidgetReference)
		{
			return false;
		}
		
		const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());
		const FVector2D FinalPosition = LocalPosition - WidgetDragOp->DragOffset;

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(WidgetDragOp->WidgetReference->Slot))
		{
			CanvasSlot->SetPosition(FinalPosition);
			WidgetDragOp->WidgetReference->SetVisibility(ESlateVisibility::Visible);
			return true;
		}
		return false;
	}

	// --- 2. If not a UI widget, check if an INVENTORY ITEM is being dropped ---
	if (USolaraqItemDragOperation* ItemDragOp = Cast<USolaraqItemDragOperation>(InOperation))
	{
		if (!ItemDragOp->ItemInfo.IsValid() || !ItemDragOp->SourceGrid)
		{
			return false;
		}
		
		UInventoryComponent* SourceInventory = ItemDragOp->SourceGrid->GetInventoryComponent();
		if (!SourceInventory)
		{
			return false;
		}
		
		const FPlacedItem ItemToDrop = ItemDragOp->ItemInfo;

		// Remove the full stack from the source inventory. This will trigger the OnInventoryUpdated delegate.
		SourceInventory->RemoveItem(ItemToDrop.ItemID, ItemToDrop.Quantity);

		// Get the player character to handle spawning the item in the world.
		if (ASolaraqCharacterPawn* PlayerPawn = Cast<ASolaraqCharacterPawn>(GetOwningPlayerPawn()))
		{
			PlayerPawn->DropItem(ItemToDrop.ItemData, ItemToDrop.Quantity);
			UE_LOG(LogTemp, Log, TEXT("  > Instructed PlayerPawn to drop item."));
			return true;
		}
		
		UE_LOG(LogTemp, Error, TEXT("  > Drop failed: OwningPlayerPawn could not be cast to ASolaraqCharacterPawn."));
		// NOTE: If the drop fails after removing the item, it will be lost. For a networked game,
		// you would need a more robust transaction system (e.g., only remove the item after the server confirms the drop).
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("  > Unhandled drop operation type."));
	return false;
}