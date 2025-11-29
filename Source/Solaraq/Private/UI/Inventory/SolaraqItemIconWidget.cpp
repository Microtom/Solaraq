// SolaraqItemIconWidget.cpp

#include "UI/Inventory/SolaraqItemIconWidget.h" 
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Items/ItemDataAssetBase.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h" 
#include "Items/SolaraqEquipmentComponent.h"
#include "UI/Inventory/SolaraqInventoryGridWidget.h"
#include "UI/Inventory/SolaraqItemDragOperation.h"

void USolaraqItemIconWidget::Initialize(const FPlacedItem& InItemInfo, USolaraqInventoryGridWidget* InOwningGrid)
{
    this->ItemInfo = InItemInfo;
    this->OwningGrid = InOwningGrid;
    
    if (!ItemInfo.ItemData) return;

    UTexture2D* IconTexture = ItemInfo.ItemData->Icon;
    
    // --- LAYOUT FIX ---
    // Ensure inner components fill the widget so the Grid controls the size.
    if (ItemButton)
    {
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(ItemButton->Slot))
        {
            CanvasSlot->SetAutoSize(false); 
            CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); 
            CanvasSlot->SetOffsets(FMargin(0.f)); 
        }

        ItemButton->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    if (ItemIcon)
    {
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(ItemIcon->Slot))
        {
            CanvasSlot->SetAutoSize(false); 
            CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); 
            CanvasSlot->SetOffsets(FMargin(0.f)); 
        }

        if (IconTexture)
        {
            ItemIcon->SetBrushFromTexture(IconTexture);
            ItemIcon->SetColorAndOpacity(FLinearColor::White);
            ItemIcon->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            ItemIcon->SetBrushFromTexture(nullptr); 
            ItemIcon->SetColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.15f, 0.8f)); 
            ItemIcon->SetVisibility(ESlateVisibility::Visible); 
        }
    }

    if (QuantityText)
    {
        if (ItemInfo.ItemData->bIsStackable && ItemInfo.Quantity > 1)
        {
            QuantityText->SetText(FText::AsNumber(ItemInfo.Quantity));
            QuantityText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            QuantityText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

FReply USolaraqItemIconWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        FEventReply Reply = UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton);
        
        // If a drag was detected, return that reply.
        if (Reply.NativeReply.IsEventHandled())
        {
            return Reply.NativeReply;
        }

        // --- FIX CLICK-THROUGH BUG ---
        // If we clicked but didn't drag, we MUST return Handled(). 
        // Otherwise, the controller gets the click and moves the character.
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

FReply USolaraqItemIconWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (OwningGrid)
        {
            APawn* OwningPawn = GetOwningPlayerPawn();
            if (!OwningPawn) return FReply::Unhandled();

            // CASE 1: The icon is in the Inventory (Backpack)
            // The grid has a reference to the InventoryComponent.
            if (UInventoryComponent* InvComp = OwningGrid->GetContextInventory())
            {
                // "Use" the item (Consumes it or Equips it)
                InvComp->UseItem(ItemInfo.ItemID);
                return FReply::Handled();
            }

            // CASE 2: The icon is in an Equipment Slot
            // The grid has NO ContextInventory, so we check the EquipmentComponent.
            if (USolaraqEquipmentComponent* EquipComp = OwningPawn->FindComponentByClass<USolaraqEquipmentComponent>())
            {
                // Check if the item we clicked is actually equipped in the slot it claims to belong to.
                if (ItemInfo.ItemData)
                {
                    EEquipmentSlot EquipmentSlotSlot = ItemInfo.ItemData->EquipmentSlot;
                    FPlacedItem EquippedItem;

                    // Does the component agree that there is an item in this slot?
                    if (EquipComp->GetItemInSlot(EquipmentSlotSlot, EquippedItem))
                    {
                        // Does the ID match the widget's ID? (Safety check)
                        if (EquippedItem.ItemID == ItemInfo.ItemID)
                        {
                            // UNEQUIP LOGIC:
                            // 1. Remove from Equipment
                            FPlacedItem RemovedItem;
                            if (EquipComp->UnequipItem(EquipmentSlotSlot, RemovedItem))
                            {
                                // 2. Add back to Inventory
                                if (UInventoryComponent* InvComp = OwningPawn->FindComponentByClass<UInventoryComponent>())
                                {
                                    int32 LeftOver = InvComp->AddItem(RemovedItem.ItemData, RemovedItem.Quantity);
                                    
                                    // Edge Case: Inventory is full?
                                    if (LeftOver > 0)
                                    {
                                        // If we couldn't add it back, strictly speaking we should re-equip it
                                        // or drop it on the ground. For now, let's just log a warning.
                                        UE_LOG(LogTemp, Warning, TEXT("Inventory full! Unequipped item lost (or implement Drop logic here)."));
                                        
                                        // Optional: Re-equip if full
                                        // FPlacedItem Dummy;
                                        // EquipComp->EquipItem(RemovedItem, Slot, Dummy);
                                    }
                                }
                            }
                            // CRITICAL: Return Handled so the character doesn't move!
                            return FReply::Handled();
                        }
                    }
                }
            }
        }
    }
    return FReply::Unhandled();
}

void USolaraqItemIconWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

    if (!ItemInfo.IsValid()) return;
    if (!OwningGrid) return;

    // 1. Calculate Target Size
    float SlotSize = OwningGrid->GetSlotPixelSize();
    float Width = ItemInfo.ItemData->Dimensions.X * SlotSize;
    float Height = ItemInfo.ItemData->Dimensions.Y * SlotSize;

    // 2. Hide original
    OwningGrid->SetItemToIgnore(this->ItemInfo.ItemID);
    OwningGrid->Redraw();

    USolaraqItemDragOperation* DragOperation = NewObject<USolaraqItemDragOperation>();
    if (!DragOperation)
    {
        OwningGrid->ClearIgnoredItem(); 
        OwningGrid->Redraw();
        return;
    }

    // 3. Create Drag Visual
    USolaraqItemIconWidget* IconVisual = CreateWidget<USolaraqItemIconWidget>(GetOwningPlayer(), GetClass());
    if(IconVisual)
    {
        IconVisual->Initialize(this->ItemInfo, nullptr);
        IconVisual->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.7f));
        
        // Use SizeBox to force the dimensions during drag
        USizeBox* SizingWrapper = NewObject<USizeBox>(this);
        SizingWrapper->SetWidthOverride(Width);
        SizingWrapper->SetHeightOverride(Height);
        
        SizingWrapper->SetContent(IconVisual);
        
        DragOperation->DefaultDragVisual = SizingWrapper;
    }

    DragOperation->Pivot = EDragPivot::MouseDown;
    DragOperation->ItemInfo = this->ItemInfo;
    DragOperation->DragOffset = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

    DragOperation->SourceGrid = OwningGrid; 
    DragOperation->HighlightWidgetClass = OwningGrid->GetHighlightWidgetClass();
    
    OutOperation = DragOperation;
}
