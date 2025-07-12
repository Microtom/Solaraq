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
	UE_LOG(LogTemp, Log, TEXT("--- WBP_ItemIcon '%s'::Initialize CALLED ---"), *WidgetName);

	// Store the item info for later use (e.g., if the player starts dragging this widget).
	this->ItemInfo = InItemInfo;
	this->OwningGrid = InOwningGrid;
	
	// --- VALIDATION LOGGING ---
	if (!ItemInfo.ItemData)
	{
		UE_LOG(LogTemp, Error, TEXT("  > ABORTING: ItemInfo.ItemData is NULL. Cannot display anything."));
		return;
	}
    UE_LOG(LogTemp, Log, TEXT("  > Item Data Asset: '%s'"), *ItemInfo.ItemData->GetName());

	if (!ItemIcon)
	{
		UE_LOG(LogTemp, Error, TEXT("  > ABORTING: 'ItemIcon' component is NULL. Check the UPROPERTY binding in the header and the widget name in WBP_ItemIcon Blueprint."));
		return;
	}
	if (!QuantityText)
	{
		UE_LOG(LogTemp, Error, TEXT("  > ABORTING: 'QuantityText' component is NULL. Check the UPROPERTY binding and widget name in WBP_ItemIcon Blueprint."));
		return;
	}
    if (!ItemButton)
    {
        UE_LOG(LogTemp, Warning, TEXT("  > NOTE: 'ItemButton' component is NULL. Clicks and drags may not work. Check binding and name in WBP_ItemIcon Blueprint."));
    }
    UE_LOG(LogTemp, Log, TEXT("  > All required components are valid."));

	// --- ICON LOGIC & LOGGING ---
	UE_LOG(LogTemp, Log, TEXT("  > Setting Icon..."));
	UTexture2D* IconTexture = ItemInfo.ItemData->Icon;
	if (IconTexture)
	{
		UE_LOG(LogTemp, Log, TEXT("    - Found valid texture '%s'. Setting brush and making image VISIBLE."), *IconTexture->GetName());
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
		UE_LOG(LogTemp, Warning, TEXT("    - Texture is NULL in the Data Asset. Displaying a solid color background as a fallback."));
		ItemIcon->SetBrushFromTexture(nullptr); // Clear any texture from the brush
		ItemIcon->SetColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.15f, 0.8f)); // Set a dark, semi-transparent background color
		ItemIcon->SetVisibility(ESlateVisibility::Visible); // Crucially, ensure it is visible
	}

	// --- QUANTITY TEXT LOGIC & LOGGING ---
	UE_LOG(LogTemp, Log, TEXT("  > Setting Quantity Text..."));
	if (ItemInfo.ItemData->bIsStackable && ItemInfo.Quantity > 1)
	{
		UE_LOG(LogTemp, Log, TEXT("    - Item is stackable and quantity is > 1. Setting text to '%d' and making it VISIBLE."), ItemInfo.Quantity);
		QuantityText->SetText(FText::AsNumber(ItemInfo.Quantity));
		QuantityText->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("    - Item is not stackable or quantity is 1. Making text COLLAPSED."));
		// Otherwise, hide the text block completely.
		QuantityText->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogTemp, Log, TEXT("--- WBP_ItemIcon '%s'::Initialize COMPLETE ---"), *WidgetName);
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
		UE_LOG(LogTemp, Error, TEXT("OwningGrid pointer is NULL in ItemIcon. Drag will not work correctly."));
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
	UE_LOG(LogTemp, Log, TEXT("Drag Detected for item: '%s'"), *ItemInfo.ItemData->DisplayName.ToString());
}