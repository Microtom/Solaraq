// SolaraqInventoryGridWidget.cpp

#include "UI/Inventory/SolaraqInventoryGridWidget.h" // Adjust path
#include "UI/Inventory/SolaraqInventorySlotWidget.h"
//#include "UI/Inventory/SolaraqItemIconWidget.h" // The simple icon widget
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Pawns/SolaraqCharacterPawn.h" // To get the inventory component
#include "Items/ItemDataAssetBase.h"
#include "UI/Inventory/SolaraqItemIconWidget.h"

void USolaraqInventoryGridWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Get and cache the inventory component.
	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		InventoryComponent = OwningPawn->FindComponentByClass<UInventoryComponent>();
	}

	// Bind our RefreshInventory function to the inventory's update delegate.
	if (InventoryComponent)
	{
		// This is the magic link. Any time the C++ inventory changes, our UI will refresh.
		InventoryComponent->OnInventoryUpdated.AddDynamic(this, &USolaraqInventoryGridWidget::RefreshInventory);
	}
    
    // Perform an initial refresh to draw the inventory for the first time.
    RefreshInventory();
}

void USolaraqInventoryGridWidget::RefreshInventory()
{
	if (!InventoryComponent || !InventorySlotClass || !ItemIconClass || !SlotCanvas || !ItemIconCanvas)
	{
		return;
	}

	SlotCanvas->ClearChildren();
	ItemIconCanvas->ClearChildren();

	// --- 1. Build a Local Grid State for fast lookups ---
	// NOTE: You'll need to add these getter functions to your UInventoryComponent class
	const int32 GridWidth = InventoryComponent->GetGridWidth();
	const int32 GridHeight = InventoryComponent->GetGridHeight();
	const TArray<FPlacedItem>& PlacedItems = InventoryComponent->GetPlacedItems();
	
	TMap<FIntPoint, FGuid> LocalGridState;
	for (const FPlacedItem& Item : PlacedItems)
	{
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
	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			const FIntPoint CurrentCoord(X, Y);
			const FGuid* OccupyingItemID = LocalGridState.Find(CurrentCoord);
			
			// For an edge to exist, this slot must be occupied.
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
			else // If the slot is empty, it's a standard 1x1 box.
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

	// --- 3. Place Item Icons on Top ---
	for (const FPlacedItem& Item : PlacedItems)
	{
		// Check if ItemData is valid before creating a widget for it.
		if(Item.ItemData)
		{
			USolaraqItemIconWidget* IconWidget = CreateWidget<USolaraqItemIconWidget>(this, ItemIconClass);
			if (IconWidget)
			{
				// *** THIS IS THE CORRECTED LINE ***
				// We pass the entire 'Item' struct, which is of type FPlacedItem.
				IconWidget->Initialize(Item); 

				UCanvasPanelSlot* CanvasSlot = ItemIconCanvas->AddChildToCanvas(IconWidget);
				CanvasSlot->SetPosition(FVector2D(Item.TopLeft.X * SlotPixelSize, Item.TopLeft.Y * SlotPixelSize));
				FVector2D ItemSize = FVector2D(Item.ItemData->Dimensions.X * SlotPixelSize, Item.ItemData->Dimensions.Y * SlotPixelSize);
				CanvasSlot->SetSize(ItemSize);
			}
		}
	}
}