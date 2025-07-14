// SolaraqItemIconWidget.cpp

#include "UI/Inventory/SolaraqItemIconWidget.h" // Adjust path as needed

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Items/ItemDataAssetBase.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/Inventory/SolaraqInventoryGridWidget.h"
#include "UI/Inventory/SolaraqItemDragOperation.h"

void USolaraqItemIconWidget::Initialize(const FPlacedItem& InItemInfo, USolaraqInventoryGridWidget* InOwningGrid)
{
	// It's helpful to have a name for the widget instance for logging clarity.
    const FString WidgetName = GetName();

	// Store the item info for later use (e.g., if the player starts dragging this widget).
	this->ItemInfo = InItemInfo;
	this->OwningGrid = InOwningGrid;
	
	// --- VALIDATION LOGGING ---
	if (!ItemInfo.ItemData)
	{
		return;
	}

	if (!ItemIcon)
	{
		return;
	}
	if (!QuantityText)
	{
		return;
	}

	// --- ICON LOGIC & LOGGING ---
	UTexture2D* IconTexture = ItemInfo.ItemData->Icon;
	if (IconTexture)
	{
		ItemIcon->SetBrushFromTexture(IconTexture);
		// Ensure tint is reset to white in case a recycled widget was previously colored.
		ItemIcon->SetColorAndOpacity(FLinearColor::White);
		ItemIcon->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		// This is a very common cause of "invisible" items. Instead of hiding the widget,
		// we'll use the UImage as a colored background as you suggested. This makes it clear
		// that the item widget is being placed correctly, but its Data Asset is missing the icon texture.
		ItemIcon->SetBrushFromTexture(nullptr); // Clear any texture from the brush
		ItemIcon->SetColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.15f, 0.8f)); // Set a dark, semi-transparent background color
		ItemIcon->SetVisibility(ESlateVisibility::Visible); // Crucially, ensure it is visible
	}

	// --- QUANTITY TEXT LOGIC & LOGGING ---
	if (ItemInfo.ItemData->bIsStackable && ItemInfo.Quantity > 1)
	{
		QuantityText->SetText(FText::AsNumber(ItemInfo.Quantity));
		QuantityText->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		// Otherwise, hide the text block completely.
		QuantityText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

FReply USolaraqItemIconWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Detect a drag operation. This will call OnDragDetected if the mouse moves far enough.
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return FReply::Unhandled();
}

void USolaraqItemIconWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (!ItemInfo.IsValid()) return;
    
    // Get the parent grid widget. This relies on the UMG hierarchy: ItemIcon -> CanvasPanel -> GridWidget
	if (!OwningGrid)
	{
		return;
	}

    // --- KEY CHANGE: Refresh the grid BEFORE creating the drag operation ---
    // 1. Tell the grid to ignore this specific item on its next refresh.
    OwningGrid->SetItemToIgnore(this->ItemInfo.ItemID);
    // 2. Trigger the refresh. The grid will now redraw without this item, showing empty 1x1 slots.
    OwningGrid->RefreshInventory();

	// --- Now, proceed with creating the drag operation as before ---
	USolaraqItemDragOperation* DragOperation = NewObject<USolaraqItemDragOperation>();
	if (!DragOperation)
	{
        OwningGrid->ClearIgnoredItem(); // Clean up if we fail
        OwningGrid->RefreshInventory();
		return;
	}

	// Create the drag visual
	USolaraqItemIconWidget* DragVisual = CreateWidget<USolaraqItemIconWidget>(GetOwningPlayer(), GetClass());
	if(DragVisual)
	{
		DragVisual->Initialize(this->ItemInfo, nullptr);
		DragVisual->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.7f));
		DragOperation->DefaultDragVisual = DragVisual;
	}

	// Configure the operation payload
	DragOperation->Pivot = EDragPivot::MouseDown;
	DragOperation->ItemInfo = this->ItemInfo;
	DragOperation->DragOffset = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

	
    // Give the operation a reference to the grid for cancellation handling
    DragOperation->SourceGrid = OwningGrid; 

	// Pass the highlight widget class from the grid to the operation so it knows what to spawn.
	DragOperation->HighlightWidgetClass = OwningGrid->GetHighlightWidgetClass();
	
	OutOperation = DragOperation;

	UE_LOG(LogTemp, Log, TEXT("ItemIcon: Drag Detected for item '%s'. Operation created."), *ItemInfo.ItemData->DisplayName.ToString());
}