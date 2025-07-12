// SolaraqInventoryGridWidget.cpp

#include "UI/Inventory/SolaraqInventoryGridWidget.h" // Adjust path
#include "UI/Inventory/SolaraqInventorySlotWidget.h"
//#include "UI/Inventory/SolaraqItemIconWidget.h" // The simple icon widget
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Pawns/SolaraqCharacterPawn.h" // To get the inventory component
#include "Items/ItemDataAssetBase.h"
#include "UI/Inventory/SolaraqItemDragOperation.h"
#include "UI/Inventory/SolaraqItemIconWidget.h"

void USolaraqInventoryGridWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogTemp, Log, TEXT("--- WBP_InventoryGrid::NativeConstruct ---"));

	// Get and cache the inventory component.
	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		InventoryComponent = OwningPawn->FindComponentByClass<UInventoryComponent>();
		if(InventoryComponent)
		{
			UE_LOG(LogTemp, Log, TEXT("  > Found InventoryComponent on Pawn '%s'."), *OwningPawn->GetName());
		}
	}

	if (!InventoryComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("  > CRITICAL: FAILED to find InventoryComponent on owning pawn!"));
		return;
	}

	// Bind our RefreshInventory function to the inventory's update delegate.
	InventoryComponent->OnInventoryUpdated.AddDynamic(this, &USolaraqInventoryGridWidget::RefreshInventory);
	UE_LOG(LogTemp, Log, TEXT("  > Bound RefreshInventory to OnInventoryUpdated delegate."));
    
	// Perform an initial refresh to draw the inventory for the first time.
	UE_LOG(LogTemp, Log, TEXT("  > Performing initial inventory refresh."));
	RefreshInventory();
}

bool USolaraqInventoryGridWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

	// Make sure we clear the ignored item ID regardless of success or failure.
	ClearIgnoredItem();
	
    // Cast the operation to our specific item drag operation class.
    USolaraqItemDragOperation* ItemDragOperation = Cast<USolaraqItemDragOperation>(InOperation);
    if (!ItemDragOperation || !InventoryComponent)
    {
        // If the cast fails or we don't have an inventory, we can't handle this drop.
        return false;
    }

    // --- Convert Mouse Position to Grid Coordinates ---
    // 1. Get the position of the drop in the local space of the grid widget.
    const FVector2D LocalDropPosition = InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());

    // 2. Account for the offset where the user initially clicked on the item.
    // This gives us the desired top-left position of the item icon.
    const FVector2D ItemTopLeftPosition = LocalDropPosition - ItemDragOperation->DragOffset;

    // 3. Convert the pixel position to a grid cell coordinate.
    // FMath::RoundToInt provides a nice 'snap' to the nearest cell, which handles
    // the drop tolerance you wanted (up to half a slot size, which is 40 pixels).
    const int32 TargetX = FMath::RoundToInt(ItemTopLeftPosition.X / SlotPixelSize);
    const int32 TargetY = FMath::RoundToInt(ItemTopLeftPosition.Y / SlotPixelSize);
    const FIntPoint TargetTopLeft(TargetX, TargetY);

    UE_LOG(LogTemp, Log, TEXT("Drop detected. Target Grid Coords: %s"), *TargetTopLeft.ToString());

    // --- Attempt to Move the Item in the Backend ---
    const bool bMoveSuccessful = InventoryComponent->MoveItem(ItemDragOperation->ItemInfo.ItemID, TargetTopLeft);

    if (bMoveSuccessful)
    {
        // The inventory component will have already broadcast the OnInventoryUpdated delegate.
        // This will trigger RefreshInventory() and redraw the item in its new location.
        UE_LOG(LogTemp, Log, TEXT("Item move successful."));
        return true; // We successfully handled the drop.
    }
    else
    {
        // The move failed (e.g., space was occupied or out of bounds).
        // The DragCancelled logic will take over automatically because we are returning false.
        // It will restore visibility on the original widget.
        UE_LOG(LogTemp, Warning, TEXT("Item move failed. Drag will be cancelled."));
    	RefreshInventory(); 
        return false; // We did not handle the drop.
    }
}

void USolaraqInventoryGridWidget::RefreshInventory()
{
	UE_LOG(LogTemp, Log, TEXT("--- WBP_InventoryGrid::RefreshInventory CALLED ---"));

	if (!InventoryComponent)
	{
        UE_LOG(LogTemp, Error, TEXT("  > ABORTING REFRESH: InventoryComponent is NULL."));
		return;
	}
    if (!InventorySlotClass)
	{
        UE_LOG(LogTemp, Error, TEXT("  > ABORTING REFRESH: 'InventorySlotClass' is not set in the WBP_InventoryGrid Blueprint defaults!"));
		return;
	}
    if (!ItemIconClass)
	{
        UE_LOG(LogTemp, Error, TEXT("  > ABORTING REFRESH: 'ItemIconClass' is not set in the WBP_InventoryGrid Blueprint defaults!"));
		return;
	}
    if (!SlotCanvas)
	{
        UE_LOG(LogTemp, Error, TEXT("  > ABORTING REFRESH: 'SlotCanvas' is NULL. Make sure a CanvasPanel with this name exists in WBP_InventoryGrid."));
		return;
	}
    if (!ItemIconCanvas)
	{
        UE_LOG(LogTemp, Error, TEXT("  > ABORTING REFRESH: 'ItemIconCanvas' is NULL. Make sure a CanvasPanel with this name exists in WBP_InventoryGrid."));
		return;
	}
    
    UE_LOG(LogTemp, Log, TEXT("  > All initial checks passed. Clearing old widgets."));
	SlotCanvas->ClearChildren();
	HighlightCanvas->ClearChildren();
	ItemIconCanvas->ClearChildren();

	// --- 1. Build a Local Grid State for fast lookups ---
	const int32 GridWidth = InventoryComponent->GetGridWidth();
	const int32 GridHeight = InventoryComponent->GetGridHeight();
	const TArray<FPlacedItem>& PlacedItems = InventoryComponent->GetPlacedItems();
	
    UE_LOG(LogTemp, Log, TEXT("  > Grid Dimensions: %d x %d. Received %d items from InventoryComponent."), GridWidth, GridHeight, PlacedItems.Num());

	TMap<FIntPoint, FGuid> LocalGridState;
	for (const FPlacedItem& Item : PlacedItems)
	{
		// If this is the item being dragged, skip it for the background calculation.
		if (ItemIDToIgnoreOnRefresh.IsValid() && Item.ItemID == ItemIDToIgnoreOnRefresh)
		{
			continue;
		}
		
		if (Item.ItemData)
		{
			for (int32 y = 0; y < Item.ItemData->Dimensions.Y; ++y)
			{
				for (int32 x = 0; x < Item.ItemData->Dimensions.X; ++x)
				{
					LocalGridState.Add(Item.TopLeft + FIntPoint(x, y), Item.ItemID);
				}
			}
		}
	}
	
	// --- 2. Procedurally Generate and Configure Background Slots ---
    UE_LOG(LogTemp, Log, TEXT("  > Generating %d background slots..."), GridWidth * GridHeight);
	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			const FIntPoint CurrentCoord(X, Y);
			const FGuid* OccupyingItemID = LocalGridState.Find(CurrentCoord);
			
			bool bIsTopEdge = false, bIsRightEdge = false, bIsBottomEdge = false, bIsLeftEdge = false;

			if (OccupyingItemID)
			{
				const FGuid* AboveItemID = LocalGridState.Find(CurrentCoord + FIntPoint(0, -1));
				const FGuid* RightItemID = LocalGridState.Find(CurrentCoord + FIntPoint(1, 0));
				const FGuid* BelowItemID = LocalGridState.Find(CurrentCoord + FIntPoint(0, 1));
				const FGuid* LeftItemID  = LocalGridState.Find(CurrentCoord + FIntPoint(-1, 0));

				bIsTopEdge    = !AboveItemID || *OccupyingItemID != *AboveItemID;
				bIsRightEdge  = !RightItemID || *OccupyingItemID != *RightItemID;
				bIsBottomEdge = !BelowItemID || *OccupyingItemID != *BelowItemID;
				bIsLeftEdge   = !LeftItemID  || *OccupyingItemID != *LeftItemID;
			}
			else
			{
				bIsTopEdge = bIsRightEdge = bIsBottomEdge = bIsLeftEdge = true;
			}
			
			USolaraqInventorySlotWidget* SlotWidget = CreateWidget<USolaraqInventorySlotWidget>(this, InventorySlotClass);
			if (SlotWidget)
			{
				SlotWidget->ConfigureSlotAppearance(bIsTopEdge, bIsRightEdge, bIsBottomEdge, bIsLeftEdge);
				UCanvasPanelSlot* CanvasSlot = SlotCanvas->AddChildToCanvas(SlotWidget);
				CanvasSlot->SetPosition(FVector2D(X * SlotPixelSize, Y * SlotPixelSize));
				CanvasSlot->SetSize(FVector2D(SlotPixelSize, SlotPixelSize));
			}
		}
	}
    UE_LOG(LogTemp, Log, TEXT("  > Finished generating background slots."));

	// --- 3. Place Item Icons on Top ---
    UE_LOG(LogTemp, Log, TEXT("  > Starting to place item icons..."));
	for (const FPlacedItem& Item : PlacedItems)
	{
		// Also skip creating an icon for the item being dragged.
		if (ItemIDToIgnoreOnRefresh.IsValid() && Item.ItemID == ItemIDToIgnoreOnRefresh)
		{
			continue;
		}
		
        // Log info for the current item being processed
        FString ItemName = Item.ItemData ? Item.ItemData->DisplayName.ToString() : TEXT("INVALID_ITEM_DATA");
        UE_LOG(LogTemp, Log, TEXT("    -> Processing item: '%s' (Qty: %d)"), *ItemName, Item.Quantity);

		if(Item.ItemData)
		{
			USolaraqItemIconWidget* IconWidget = CreateWidget<USolaraqItemIconWidget>(this, ItemIconClass);
			if (IconWidget)
			{
                UE_LOG(LogTemp, Log, TEXT("      - Successfully created WBP_ItemIcon widget."));
				IconWidget->Initialize(Item, this); 
                UE_LOG(LogTemp, Log, TEXT("      - Initialized widget with item data."));

				UCanvasPanelSlot* CanvasSlot = ItemIconCanvas->AddChildToCanvas(IconWidget);
                UE_LOG(LogTemp, Log, TEXT("      - Added widget to ItemIconCanvas."));
				
                FVector2D ItemPosition = FVector2D(Item.TopLeft.X * SlotPixelSize, Item.TopLeft.Y * SlotPixelSize);
				FVector2D ItemSize = FVector2D(Item.ItemData->Dimensions.X * SlotPixelSize, Item.ItemData->Dimensions.Y * SlotPixelSize);
                
                CanvasSlot->SetPosition(ItemPosition);
				CanvasSlot->SetSize(ItemSize);
                UE_LOG(LogTemp, Log, TEXT("      - Set position to %s and size to %s."), *ItemPosition.ToString(), *ItemSize.ToString());

				CanvasSlot->SetZOrder(1); 
				
			}
            else
            {
                // THIS IS THE MOST LIKELY POINT OF FAILURE
                UE_LOG(LogTemp, Error, TEXT("      - FAILED to create Item Icon Widget! 'ItemIconClass' is likely not set correctly in the WBP_InventoryGrid Blueprint!"));
            }
		}
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("    -> SKIPPING item because its ItemData is NULL."));
        }
	}
    UE_LOG(LogTemp, Log, TEXT("  > Finished placing item icons."));
    UE_LOG(LogTemp, Log, TEXT("--- WBP_InventoryGrid::RefreshInventory COMPLETE ---"));
}

void USolaraqInventoryGridWidget::SetItemToIgnore(const FGuid& ItemID)
{
	ItemIDToIgnoreOnRefresh = ItemID;
}

void USolaraqInventoryGridWidget::ClearIgnoredItem()
{
	ItemIDToIgnoreOnRefresh.Invalidate();
}
