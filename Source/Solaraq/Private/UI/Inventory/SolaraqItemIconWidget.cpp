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

            // CASE 1: The icon is in a Grid (Inventory or Container)
            if (UInventoryComponent* ContextInv = OwningGrid->GetContextInventory())
            {
                AActor* InvOwner = ContextInv->GetOwner();
                
                // Sub-Case A: It's the Player's Own Inventory -> Use/Equip the item
                if (InvOwner == OwningPawn)
                {
                    ContextInv->UseItem(ItemInfo.ItemID);
                }
                // Sub-Case B: It's a Container (Owner is not the pawn) -> Loot the item
                else 
                {
                    if (UInventoryComponent* PlayerInv = OwningPawn->FindComponentByClass<UInventoryComponent>())
                    {
                        // 1. Attempt to add item to player inventory (Auto-find spot logic)
                        // Note: AddItem returns the quantity REMAINING (that couldn't be added)
                        int32 Remainder = PlayerInv->AddItem(ItemInfo.ItemData, ItemInfo.Quantity);
                        
                        // 2. Calculate how many were successfully moved
                        int32 AmountMoved = ItemInfo.Quantity - Remainder;

                        if (AmountMoved > 0)
                        {
                            // 3. Remove the successfully moved amount from the container
                            ContextInv->RemoveItem(ItemInfo.ItemID, AmountMoved);
                            
                            UE_LOG(LogTemp, Log, TEXT("Looted %d x %s from container."), AmountMoved, *ItemInfo.ItemData->GetName());
                        }
                        else
                        {
                             UE_LOG(LogTemp, Warning, TEXT("Cannot loot: Inventory Full."));
                        }
                    }
                }
                
                return FReply::Handled();
            }

            // CASE 2: The icon is in an Equipment Slot (No Context Inventory on Grid)
            // The grid has NO ContextInventory, so we check the EquipmentComponent.
            if (USolaraqEquipmentComponent* EquipComp = OwningPawn->FindComponentByClass<USolaraqEquipmentComponent>())
            {
                // Check if the item we clicked is actually equipped in the slot it claims to belong to.
                if (ItemInfo.ItemData)
                {
                    EEquipmentSlot TargetSlot = ItemInfo.ItemData->EquipmentSlot;
                    FPlacedItem EquippedItem;

                    // Does the component agree that there is an item in this slot?
                    if (EquipComp->GetItemInSlot(TargetSlot, EquippedItem))
                    {
                        // Does the ID match the widget's ID? (Safety check)
                        if (EquippedItem.ItemID == ItemInfo.ItemID)
                        {
                            // UNEQUIP LOGIC:
                            // 1. Remove from Equipment
                            FPlacedItem RemovedItem;
                            if (EquipComp->UnequipItem(TargetSlot, RemovedItem))
                            {
                                // 2. Add back to Inventory
                                if (UInventoryComponent* InvComp = OwningPawn->FindComponentByClass<UInventoryComponent>())
                                {
                                    int32 LeftOver = InvComp->AddItem(RemovedItem.ItemData, RemovedItem.Quantity);
                                    
                                    // Edge Case: Inventory is full?
                                    if (LeftOver > 0)
                                    {
                                        UE_LOG(LogTemp, Warning, TEXT("Inventory full! Unequipped item lost (or implement Drop logic here)."));
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
