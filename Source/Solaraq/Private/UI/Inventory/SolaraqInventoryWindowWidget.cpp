// SolaraqInventoryWindowWidget.cpp
#include "UI/Inventory/SolaraqInventoryWindowWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Items/SolaraqEquipmentComponent.h"
#include "UI/Inventory/SolaraqInventoryGridWidget.h"
#include "Pawns/SolaraqCharacterPawn.h"
#include "UI/SolaraqWidgetDragOperation.h"

void USolaraqInventoryWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. Find Inventory Component
	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		InventoryComp = OwningPawn->FindComponentByClass<UInventoryComponent>();
	}

	// 2. Setup Grid
	if (InventoryComp && BackpackGrid)
	{
		BackpackGrid->ConfigureGrid(InventoryComp->GetGridWidth(), InventoryComp->GetGridHeight());
		BackpackGrid->SetContextInventory(InventoryComp);
		
		// Remove existing binds first to be safe
		InventoryComp->OnInventoryUpdated.RemoveDynamic(this, &USolaraqInventoryWindowWidget::RefreshWindow);
		InventoryComp->OnInventoryUpdated.AddDynamic(this, &USolaraqInventoryWindowWidget::RefreshWindow);
		
		BackpackGrid->OnItemDrop.RemoveDynamic(this, &USolaraqInventoryWindowWidget::HandleGridDrop);
		BackpackGrid->OnItemDrop.AddDynamic(this, &USolaraqInventoryWindowWidget::HandleGridDrop);
		
		RefreshWindow();
	}

	// 3. Setup Close Button (Restored)
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &USolaraqInventoryWindowWidget::CloseWindow);
		CloseButton->OnClicked.AddDynamic(this, &USolaraqInventoryWindowWidget::CloseWindow);
	}
}

FReply USolaraqInventoryWindowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
const FPointerEvent& InMouseEvent)
{
	// Try to detect a drag first
	FEventReply Reply = UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton);
    
	// If a drag operation started, return that handled reply
	if (Reply.NativeReply.IsEventHandled())
	{
		return Reply.NativeReply;
	}

	// If we are here, we clicked on the window but didn't drag. 
	// We MUST return Handled() to prevent the PlayerController from receiving this click 
	// and moving the character.
	return FReply::Handled();
}

void USolaraqInventoryWindowWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	USolaraqWidgetDragOperation* DragOp = NewObject<USolaraqWidgetDragOperation>();
	if (DragOp)
	{
		DragOp->WidgetReference = this;

		// --- CALCULATION & DEBUGGING ---
		FVector2D WidgetAbsPos = InGeometry.GetAbsolutePosition();
		FVector2D MouseAbsPos = InMouseEvent.GetScreenSpacePosition();
		DragOp->DragOffset = MouseAbsPos - WidgetAbsPos;

		UE_LOG(LogTemp, Warning, TEXT("DRAG START | MouseAbs: %s | WidgetAbs: %s | Calculated Offset: %s"), 
			*MouseAbsPos.ToString(), 
			*WidgetAbsPos.ToString(), 
			*DragOp->DragOffset.ToString());
		// -------------------------------

		USolaraqInventoryWindowWidget* DragVisual = CreateWidget<USolaraqInventoryWindowWidget>(GetOwningPlayer(), GetClass());
		if (DragVisual)
		{
			DragOp->DefaultDragVisual = DragVisual;
		}
		else
		{
			DragOp->DefaultDragVisual = this;
		}

		DragOp->Pivot = EDragPivot::MouseDown; 
		this->SetVisibility(ESlateVisibility::Hidden); 
		
		OutOperation = DragOp;
	}
}

void USolaraqInventoryWindowWidget::RefreshWindow()
{
	UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] Window RefreshWindow Called."));
	if (InventoryComp && BackpackGrid)
	{
		const TArray<FPlacedItem>& Items = InventoryComp->GetPlacedItems();
		UE_LOG(LogTemp, Warning, TEXT("[DEBUG_INV] Pushing %d items to Grid Widget."), Items.Num());
		BackpackGrid->UpdateState(Items);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[DEBUG_INV] RefreshWindow aborted. Comp or Grid is null."));
	}
}

void USolaraqInventoryWindowWidget::HandleGridDrop(const FPlacedItem& DroppedItem, USolaraqInventoryGridWidget* SourceGrid, USolaraqInventoryGridWidget* TargetGrid, FIntPoint TargetCoord)
{
	if (!InventoryComp) return;

    // CASE 1: Moving item within the Backpack (Reorganizing)
    if (SourceGrid == TargetGrid)
    {
        InventoryComp->MoveItem(DroppedItem.ItemID, TargetCoord);
        return;
    }

    // CASE 2: Dragging FROM Equipment TO Backpack
    // Since Equipment grids don't have a ContextInventory set, we can check for that,
    // or we can simply check if the item exists in the inventory.
    
    // Attempt to move. If MoveItem returns false because ID isn't found, it implies it's external.
    // However, clean logic is better:
    
    if (APawn* OwningPawn = GetOwningPlayerPawn())
    {
        if (USolaraqEquipmentComponent* EquipComp = OwningPawn->FindComponentByClass<USolaraqEquipmentComponent>())
        {
            // We assume that if it's not from the same grid, it's from equipment (in this specific window setup).
            // Logic: 
            // 1. Remove from Equipment Slot
            // 2. Add to Inventory at TargetCoord

            FPlacedItem RemovedItem;
            // Use the data in DroppedItem to know which slot to unequip from
            if (EquipComp->UnequipItem(DroppedItem.ItemData->EquipmentSlot, RemovedItem))
            {
                // Try to place it at the specific drop location
                bool bAdded = InventoryComp->AddItemAt(RemovedItem, TargetCoord);

                if (!bAdded)
                {
                    // If target spot was full (e.g. dropped on top of another item), 
                    // fallback to standard AddItem (finds first free spot)
                    // or re-equip it (cancel drop).
                    
                    // Let's try standard add (auto-find spot)
                    int32 Remaining = InventoryComp->AddItem(RemovedItem.ItemData, RemovedItem.Quantity);
                    
                    if (Remaining > 0)
                    {
                        // Inventory completely full? Put it back on equipment.
                        FPlacedItem Dummy;
                        EquipComp->EquipItem(RemovedItem, RemovedItem.ItemData->EquipmentSlot, Dummy);
                    }
                }
            }
        }
    }
}

void USolaraqInventoryWindowWidget::CloseWindow()
{
	SetVisibility(ESlateVisibility::Collapsed);
	
	// Optional: If you want to switch input modes back to game-only immediately:
	// if (APlayerController* PC = GetOwningPlayer()) {
	//     PC->SetInputMode(FInputModeGameOnly());
	//     PC->bShowMouseCursor = false;
	// }

	if (OnCloseRequested.IsBound())
	{
		OnCloseRequested.Broadcast();
	}
}

// Add this to handle Double Clicks on the window background so they don't leak either
FReply USolaraqInventoryWindowWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Consume the double click
	return FReply::Handled();
}
