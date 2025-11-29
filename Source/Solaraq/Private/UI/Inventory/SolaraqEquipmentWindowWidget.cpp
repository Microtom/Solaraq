// SolaraqEquipmentWindowWidget.cpp
#include "UI/Inventory/SolaraqEquipmentWindowWidget.h"
#include "UI/Inventory/SolaraqInventoryGridWidget.h"
#include "Items/SolaraqEquipmentComponent.h"
#include "Items/InventoryComponent.h"
#include "UI/Common/SolaraqCircularButton.h" 
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/SolaraqWidgetDragOperation.h" // Required for window movement

void USolaraqEquipmentWindowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (APawn* OwningPawn = GetOwningPlayerPawn())
    {
        EquipmentComp = OwningPawn->FindComponentByClass<USolaraqEquipmentComponent>();
        InventoryComp = OwningPawn->FindComponentByClass<UInventoryComponent>();
    }

    // Bind the Close Button
    // Note: USolaraqCircularButton inherits from UButton, so OnClicked works the same.
    if (CloseButton)
    {
        CloseButton->OnClicked.AddDynamic(this, &USolaraqEquipmentWindowWidget::CloseWindow);
    }

    // Helper setup function
    auto SetupSlot = [&](USolaraqInventoryGridWidget* Grid, EEquipmentSlot EquipSlot, int32 W, int32 H)
    {
        if (Grid)
        {
            // Configure View
            Grid->ConfigureGrid(W, H);
            
            // Map for Logic lookup later
            GridToSlotMap.Add(Grid, EquipSlot);
            
            // Bind Logic
            Grid->OnItemDrop.RemoveDynamic(this, &USolaraqEquipmentWindowWidget::HandleEquipmentDrop);
            Grid->OnItemDrop.AddDynamic(this, &USolaraqEquipmentWindowWidget::HandleEquipmentDrop);
        }
    };

    // Configure the Grids
    SetupSlot(Grid_Head, EEquipmentSlot::Head, 2, 2);
    SetupSlot(Grid_Body, EEquipmentSlot::Body, 2, 3);
    SetupSlot(Grid_MainHand, EEquipmentSlot::MainHand, 2, 6);
    SetupSlot(Grid_OffHand, EEquipmentSlot::OffHand, 2, 4);

    if (EquipmentComp)
    {
        EquipmentComp->OnEquipmentUpdated.RemoveDynamic(this, &USolaraqEquipmentWindowWidget::RefreshWindow);
        EquipmentComp->OnEquipmentUpdated.AddDynamic(this, &USolaraqEquipmentWindowWidget::RefreshWindow);
        RefreshWindow();
    }
}

FReply USolaraqEquipmentWindowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Detect if the user wants to drag the window
    FEventReply Reply = UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton);
    
    // CRITICAL: We return Handled() here. 
    // This tells the PlayerController that the UI consumed the click.
    // Consequently, the HitResultUnderCursor in the Controller will NOT trigger movement.
    if (Reply.NativeReply.IsEventHandled())
    {
        return Reply.NativeReply;
    }

    // Even if we aren't dragging, we handle the mouse down so the click doesn't fall through to the world.
    return FReply::Handled();
}

void USolaraqEquipmentWindowWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

    // Reuse the logic from the Inventory Window to enable moving this window
    USolaraqWidgetDragOperation* DragOp = NewObject<USolaraqWidgetDragOperation>();
    if (DragOp)
    {
        DragOp->WidgetReference = this;

        // Calculate offset so the window doesn't snap to the mouse corner
        FVector2D WidgetAbsPos = InGeometry.GetAbsolutePosition();
        FVector2D MouseAbsPos = InMouseEvent.GetScreenSpacePosition();
        DragOp->DragOffset = MouseAbsPos - WidgetAbsPos;

        // Create a visual drag (ghost)
        USolaraqEquipmentWindowWidget* DragVisual = CreateWidget<USolaraqEquipmentWindowWidget>(GetOwningPlayer(), GetClass());
        if (DragVisual)
        {
            // Optional: Copy state to visual if you want the visual to look exactly like the current window
            DragOp->DefaultDragVisual = DragVisual;
        }
        else
        {
            DragOp->DefaultDragVisual = this;
        }

        DragOp->Pivot = EDragPivot::MouseDown; 
        
        // Hide the original window while dragging
        this->SetVisibility(ESlateVisibility::Hidden); 
        
        OutOperation = DragOp;
    }
}

void USolaraqEquipmentWindowWidget::CloseWindow()
{    
    // Broadcast to controller so it can save position and cleanup
    if (OnCloseRequested.IsBound())
    {
        OnCloseRequested.Broadcast();
    }
}

void USolaraqEquipmentWindowWidget::RefreshWindow()
{
    if (!EquipmentComp) return;

    // Loop through our mapped grids and push data to them
    for (auto& Elem : GridToSlotMap)
    {
        USolaraqInventoryGridWidget* Grid = Elem.Key;
        EEquipmentSlot EquipSlot = Elem.Value;

        TArray<FPlacedItem> ItemsForGrid;
        FPlacedItem ItemInSlot;
        
        // Get data from Model
        if (EquipmentComp->GetItemInSlot(EquipSlot, ItemInSlot))
        {
            // Ensure visual position is 0,0 for the grid
            ItemInSlot.TopLeft = FIntPoint(0, 0); 
            ItemsForGrid.Add(ItemInSlot);
        }

        // Push data to View
        Grid->UpdateState(ItemsForGrid);
    }
}

void USolaraqEquipmentWindowWidget::HandleEquipmentDrop(const FPlacedItem& DroppedItem, USolaraqInventoryGridWidget* SourceGrid, USolaraqInventoryGridWidget* TargetGrid, FIntPoint TargetCoord)
{
    if (!TargetGrid || !DroppedItem.ItemData || !InventoryComp || !EquipmentComp) return;
    if (!GridToSlotMap.Contains(TargetGrid)) return;

    EEquipmentSlot TargetSlot = GridToSlotMap[TargetGrid];

    // Basic Validation
    if (DroppedItem.ItemData->EquipmentSlot != TargetSlot)
    {
        return; 
    }

    // --- LOGIC SPLIT BASED ON SOURCE ---
    
    bool bCameFromBackpack = false;
    
    // 1. Determine Source
    if (SourceGrid && SourceGrid->GetContextInventory())
    {
        // If the grid has a context inventory, it's the backpack
        bCameFromBackpack = true;
    }

    // 2. Remove from Source
    if (bCameFromBackpack)
    {
        // Remove from Inventory
        InventoryComp->RemoveItem(DroppedItem.ItemID, DroppedItem.Quantity);
    }
    else
    {
        // Came from another Equipment Slot?
        // Check if we are just dropping it back onto the same slot (Drag Head -> Drop Head)
        if (SourceGrid == TargetGrid)
        {
            // Do nothing, we just put it back where it was.
            return;
        }

        // If dragging from one equip slot to another (e.g. Ring 1 to Ring 2, if supported)
        if (SourceGrid && GridToSlotMap.Contains(SourceGrid))
        {
            EEquipmentSlot SourceSlot = GridToSlotMap[SourceGrid];
            FPlacedItem RemovedItem;
            EquipmentComp->UnequipItem(SourceSlot, RemovedItem);
        }
    }

    // 3. Equip to Target
    FPlacedItem PreviousItem;
    bool bSuccess = EquipmentComp->EquipItem(DroppedItem, TargetSlot, PreviousItem);

    if (bSuccess)
    {
        // 4. Handle Swap (Put old item into inventory)
        if (PreviousItem.IsValid())
        {
            // Edge Case: If we swapped with ourselves (should be caught by Source==Target check above, but safe to check ID)
            if (PreviousItem.ItemID != DroppedItem.ItemID)
            {
                InventoryComp->AddItem(PreviousItem.ItemData, PreviousItem.Quantity);
            }
        }
    }
    else
    {
        // 5. Revert on Failure
        if (bCameFromBackpack)
        {
            // Put it back in inventory
            InventoryComp->AddItem(DroppedItem.ItemData, DroppedItem.Quantity);
        }
        else
        {
            // Put it back in the source equipment slot
             if (SourceGrid && GridToSlotMap.Contains(SourceGrid))
             {
                 EEquipmentSlot SourceSlot = GridToSlotMap[SourceGrid];
                 FPlacedItem Dummy;
                 EquipmentComp->EquipItem(DroppedItem, SourceSlot, Dummy);
             }
        }
    }
}