#include "UI/Inventory/SolaraqContainerWindowWidget.h"
#include "UI/Inventory/SolaraqInventoryGridWidget.h"
#include "Actors/SolaraqContainerBase.h"
#include "Components/Button.h"
#include "UI/SolaraqWidgetDragOperation.h" // Assuming you use this for window dragging

void USolaraqContainerWindowWidget::InitContainerWindow(ASolaraqContainerBase* InContainerActor)
{
	LinkedContainer = InContainerActor;
	if (LinkedContainer)
	{
		ContainerInventory = LinkedContainer->InventoryComponent;
	}

	if (ContainerInventory && ContainerGrid)
	{
		ContainerGrid->ConfigureGrid(ContainerInventory->GetGridWidth(), ContainerInventory->GetGridHeight());
		ContainerGrid->SetContextInventory(ContainerInventory);

		// Bind Updates
		ContainerInventory->OnInventoryUpdated.RemoveAll(this); // Clear old binds
		ContainerInventory->OnInventoryUpdated.AddDynamic(ContainerGrid, &USolaraqInventoryGridWidget::Redraw); // Direct bind or via refresh function
		
		// Initial Draw
		ContainerGrid->UpdateState(ContainerInventory->GetPlacedItems());

		// Drop Logic
		ContainerGrid->OnItemDrop.RemoveDynamic(this, &USolaraqContainerWindowWidget::HandleGridDrop);
		ContainerGrid->OnItemDrop.AddDynamic(this, &USolaraqContainerWindowWidget::HandleGridDrop);
	}
}

void USolaraqContainerWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &USolaraqContainerWindowWidget::CloseWindow);
	}
}

void USolaraqContainerWindowWidget::HandleGridDrop(const FPlacedItem& DroppedItem, USolaraqInventoryGridWidget* SourceGrid, USolaraqInventoryGridWidget* TargetGrid, FIntPoint TargetCoord)
{
	if (!ContainerInventory) return;

	UInventoryComponent* SourceComp = SourceGrid->GetContextInventory();

	// 1. Reorganizing inside the container
	if (SourceComp == ContainerInventory)
	{
		ContainerInventory->MoveItem(DroppedItem.ItemID, TargetCoord);
	}
	// 2. Dragging FROM Player Backpack TO Container
	else if (SourceComp)
	{
		// Use the new Transfer function
		SourceComp->TransferItemTo(ContainerInventory, DroppedItem.ItemID, TargetCoord);
	}
}

void USolaraqContainerWindowWidget::CloseWindow()
{
	if (LinkedContainer)
	{
		LinkedContainer->CloseContainer();
	}
	RemoveFromParent();
}

// Optional: Add NativeOnDragDetected here if you want the window to be movable like the player inventory
bool USolaraqContainerWindowWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    // Copy drag logic from PlayerInventoryWindow if you want window dragging
    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}