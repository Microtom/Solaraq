#include "UI/Inventory/SolaraqContainerWindowWidget.h"
#include "UI/Inventory/SolaraqInventoryGridWidget.h"
#include "Actors/Interactables/SolaraqContainerBase.h"
#include "Components/Button.h"
#include "UI/SolaraqWidgetDragOperation.h" 
#include "Blueprint/WidgetBlueprintLibrary.h"

void USolaraqContainerWindowWidget::InitContainerWindow(ASolaraqContainerBase* InContainerActor)
{
	// Just set the data references here. 
	// We defer visual setup to NativeConstruct to ensure the widget tree is ready.
	LinkedContainer = InContainerActor;
	if (LinkedContainer)
	{
		ContainerInventory = LinkedContainer->InventoryComponent;
	}
}

void USolaraqContainerWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. Setup Close Button
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &USolaraqContainerWindowWidget::CloseWindow);
		CloseButton->OnClicked.AddDynamic(this, &USolaraqContainerWindowWidget::CloseWindow);
	}

	// 2. Setup Loot All Button (NEW)
	if (LootAllButton)
	{
		LootAllButton->OnClicked.RemoveDynamic(this, &USolaraqContainerWindowWidget::OnLootAllClicked);
		LootAllButton->OnClicked.AddDynamic(this, &USolaraqContainerWindowWidget::OnLootAllClicked);
	}

	// 2. Setup Grid
	// We do this in NativeConstruct because that's when the widget tree is guaranteed to be accessible.
	if (ContainerInventory && ContainerGrid)
	{
		UE_LOG(LogTemp, Warning, TEXT("ContainerWindow NativeConstruct: Configuring Grid. Size: %d x %d"), ContainerInventory->GetGridWidth(), ContainerInventory->GetGridHeight());
		
		// CRITICAL FIX: Force the child widget to update its layout immediately.
		// This ensures 'SlotCanvas' inside the grid is created/bound before we try to use it,
		// which is common issue with Drag Visuals or dynamic widgets.
		ContainerGrid->ForceLayoutPrepass();

		ContainerGrid->ConfigureGrid(ContainerInventory->GetGridWidth(), ContainerInventory->GetGridHeight());
		ContainerGrid->SetContextInventory(ContainerInventory);

		// Bind Updates
		// We remove bindings from the Grid instance to prevent double-binding if the window is reused
		ContainerInventory->OnInventoryUpdated.RemoveDynamic(ContainerGrid, &USolaraqInventoryGridWidget::Redraw); 
		ContainerInventory->OnInventoryUpdated.AddDynamic(ContainerGrid, &USolaraqInventoryGridWidget::Redraw); 
		
		// Initial Draw
		ContainerGrid->UpdateState(ContainerInventory->GetPlacedItems());

		// Drop Logic
		ContainerGrid->OnItemDrop.RemoveDynamic(this, &USolaraqContainerWindowWidget::HandleGridDrop);
		ContainerGrid->OnItemDrop.AddDynamic(this, &USolaraqContainerWindowWidget::HandleGridDrop);
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
		SourceComp->TransferItemTo(ContainerInventory, DroppedItem.ItemID, TargetCoord);
	}
}

void USolaraqContainerWindowWidget::OnLootAllClicked()
{
	if (!ContainerInventory) return;

	// 1. Get the Player Pawn
	APawn* OwningPawn = GetOwningPlayerPawn();
	if (!OwningPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("LootAll failed: No Owning Pawn found."));
		return;
	}

	// 2. Find the Player's Inventory Component
	// Assuming the standard player character has the component
	UInventoryComponent* PlayerInventory = OwningPawn->FindComponentByClass<UInventoryComponent>();
    
	if (PlayerInventory)
	{
		// 3. Execute the transfer
		ContainerInventory->TransferAllItemsTo(PlayerInventory);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("LootAll failed: Player Pawn does not have an InventoryComponent."));
	}
}

void USolaraqContainerWindowWidget::CloseWindow()
{
	if (LinkedContainer)
	{
		LinkedContainer->CloseContainer();
	}

	if (OnCloseRequested.IsBound())
	{
		OnCloseRequested.Broadcast();
	}
	
	RemoveFromParent();
}

// --- Dragging Logic ---

FReply USolaraqContainerWindowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FEventReply Reply = UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton);
	if (Reply.NativeReply.IsEventHandled())
	{
		return Reply.NativeReply;
	}
	return FReply::Handled();
}

void USolaraqContainerWindowWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	USolaraqWidgetDragOperation* DragOp = NewObject<USolaraqWidgetDragOperation>();
	if (DragOp)
	{
		DragOp->WidgetReference = this;

		FVector2D WidgetAbsPos = InGeometry.GetAbsolutePosition();
		FVector2D MouseAbsPos = InMouseEvent.GetScreenSpacePosition();
		DragOp->DragOffset = MouseAbsPos - WidgetAbsPos;

		// Create a visual copy for the drag
		USolaraqContainerWindowWidget* DragVisual = CreateWidget<USolaraqContainerWindowWidget>(GetOwningPlayer(), GetClass());
		if (DragVisual)
		{
			// Init the data on the drag visual
			if (LinkedContainer)
			{
				DragVisual->InitContainerWindow(LinkedContainer);
			}
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

bool USolaraqContainerWindowWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}