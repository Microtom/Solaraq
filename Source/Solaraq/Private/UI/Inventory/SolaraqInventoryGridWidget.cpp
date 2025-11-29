// SolaraqInventoryGridWidget.cpp
#include "UI/Inventory/SolaraqInventoryGridWidget.h"
#include "UI/Inventory/SolaraqItemDragOperation.h"
#include "UI/Inventory/SolaraqInventorySlotWidget.h"
#include "UI/Inventory/SolaraqItemIconWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Items/ItemDataAssetBase.h"

void USolaraqInventoryGridWidget::ConfigureGrid(int32 InWidth, int32 InHeight, float InSlotSize)
{
	GridWidth = InWidth;
	GridHeight = InHeight;
	SlotPixelSize = InSlotSize;
    
	SlotWidgets.Empty(); // Clear existing cache

	if (SlotCanvas && InventorySlotClass)
	{
		SlotCanvas->ClearChildren();
		for (int32 Y = 0; Y < GridHeight; ++Y)
		{
			for (int32 X = 0; X < GridWidth; ++X)
			{
				USolaraqInventorySlotWidget* SlotWidget = CreateWidget<USolaraqInventorySlotWidget>(this, InventorySlotClass);
				if (SlotWidget)
				{
					// Default to empty box (all borders true)
					SlotWidget->ConfigureSlotAppearance(true, true, true, true);
                    
					UCanvasPanelSlot* CanvasSlot = SlotCanvas->AddChildToCanvas(SlotWidget);
					CanvasSlot->SetPosition(FVector2D(X * SlotPixelSize, Y * SlotPixelSize));
					CanvasSlot->SetSize(FVector2D(SlotPixelSize, SlotPixelSize));
                    
					// Cache it for Redraw updates
					SlotWidgets.Add(SlotWidget);
				}
			}
		}
	}
}

void USolaraqInventoryGridWidget::SetContextInventory(UInventoryComponent* InInventory)
{
	ContextInventory = InInventory;
}

void USolaraqInventoryGridWidget::UpdateState(const TArray<FPlacedItem>& InItems)
{
	CachedItems = InItems;
	Redraw();
}

void USolaraqInventoryGridWidget::Redraw()
{
    // --- PART 1: Calculate Grid State for Backgrounds ---
    
    // Create a temporary map representing the grid: Coord -> ItemID
    TMap<FIntPoint, FGuid> OccupiedSlots;

    for (const FPlacedItem& Item : CachedItems)
    {
        // Ghosting: Treat ignored item as if it doesn't exist
        if (ItemIDToIgnore.IsValid() && Item.ItemID == ItemIDToIgnore) continue;
        if (!Item.ItemData) continue;

        // Mark all cells occupied by this item
        for (int32 Y = 0; Y < Item.ItemData->Dimensions.Y; ++Y)
        {
            for (int32 X = 0; X < Item.ItemData->Dimensions.X; ++X)
            {
                OccupiedSlots.Add(Item.TopLeft + FIntPoint(X, Y), Item.ItemID);
            }
        }
    }

    // Update the appearance of every background slot
    // SlotWidgets is a flat array, ordered Row by Row (Y then X)
    for (int32 i = 0; i < SlotWidgets.Num(); ++i)
    {
        USolaraqInventorySlotWidget* SlotWidget = SlotWidgets[i];
        if (!SlotWidget) continue;

        int32 X = i % GridWidth;
        int32 Y = i / GridWidth;
        FIntPoint Current(X, Y);

        bool bIsOccupied = OccupiedSlots.Contains(Current);
        FGuid CurrentID = bIsOccupied ? OccupiedSlots[Current] : FGuid(); // Empty GUID if not occupied

        // Helper to check neighbors
        // A border exists if:
        // 1. We are empty (always borders), OR
        // 2. We are occupied, and the neighbor is EITHER empty OR has a DIFFERENT Item ID.
        auto NeedsBorder = [&](FIntPoint NeighborCoord) -> bool
        {
            if (!bIsOccupied) return true; // Empty slots always have borders

            // Check boundaries
            if (NeighborCoord.X < 0 || NeighborCoord.Y < 0 || NeighborCoord.X >= GridWidth || NeighborCoord.Y >= GridHeight)
                return true; // Edge of grid is always a border

            if (!OccupiedSlots.Contains(NeighborCoord)) return true; // Neighbor is empty

            FGuid NeighborID = OccupiedSlots[NeighborCoord];
            return NeighborID != CurrentID; // True if IDs don't match (draw border separation)
        };

        bool bTop    = NeedsBorder(Current + FIntPoint(0, -1));
        bool bRight  = NeedsBorder(Current + FIntPoint(1, 0));
        bool bBottom = NeedsBorder(Current + FIntPoint(0, 1));
        bool bLeft   = NeedsBorder(Current + FIntPoint(-1, 0));

        SlotWidget->ConfigureSlotAppearance(bTop, bRight, bBottom, bLeft);
    }


    // --- PART 2: Draw Item Icons ---

    if (!ItemIconCanvas || !ItemIconClass) return;
    ItemIconCanvas->ClearChildren();

    for (const FPlacedItem& Item : CachedItems)
    {
        if (ItemIDToIgnore.IsValid() && Item.ItemID == ItemIDToIgnore) continue;

        if (Item.ItemData)
        {
            USolaraqItemIconWidget* IconWidget = CreateWidget<USolaraqItemIconWidget>(this, ItemIconClass);
            if (IconWidget)
            {
                IconWidget->Initialize(Item, this);
                UCanvasPanelSlot* CanvasSlot = ItemIconCanvas->AddChildToCanvas(IconWidget);
                
                FVector2D Pos(Item.TopLeft.X * SlotPixelSize, Item.TopLeft.Y * SlotPixelSize);
                FVector2D Size(Item.ItemData->Dimensions.X * SlotPixelSize, Item.ItemData->Dimensions.Y * SlotPixelSize);

                CanvasSlot->SetPosition(Pos);
                CanvasSlot->SetSize(Size);
            }
        }
    }
}

bool USolaraqInventoryGridWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	
	ClearIgnoredItem();
	Redraw(); // Re-calculates borders immediately so the ghosted item reappears

	USolaraqItemDragOperation* DragOp = Cast<USolaraqItemDragOperation>(InOperation);
	if (!DragOp) return false;

	const FVector2D LocalDropPos = InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());
	const FVector2D IconTopLeft = LocalDropPos - DragOp->DragOffset;
	
	const int32 X = FMath::RoundToInt(IconTopLeft.X / SlotPixelSize);
	const int32 Y = FMath::RoundToInt(IconTopLeft.Y / SlotPixelSize);
	
	OnItemDrop.Broadcast(DragOp->ItemInfo, DragOp->SourceGrid, this, FIntPoint(X, Y));

	return true;
}

void USolaraqInventoryGridWidget::SetItemToIgnore(const FGuid& ItemID)
{
	ItemIDToIgnore = ItemID;
}

void USolaraqInventoryGridWidget::ClearIgnoredItem()
{
	ItemIDToIgnore.Invalidate();
}