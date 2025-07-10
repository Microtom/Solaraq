#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Items/InventoryComponent.h"
#include "SolaraqItemDragOperation.generated.h"

class USolaraqItemIconWidget;

/**
 *  Carries the payload for dragging an inventory item.
 */
UCLASS()
class SOLARAQ_API USolaraqItemDragOperation : public UDragDropOperation
{
	GENERATED_BODY()
	
public:
	// The item data and its original position that is being dragged.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Solaraq|Drag Operation")
	FPlacedItem ItemInfo;

	// A reference to the widget that initiated the drag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Solaraq|Drag Operation")
	TObjectPtr<USolaraqItemIconWidget> OriginalWidget;

	// The offset of the mouse cursor from the top-left of the widget when the drag started.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Solaraq|Drag Operation")
	FVector2D DragOffset;
	
	// Overridden from UDragDropOperation.
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;
};