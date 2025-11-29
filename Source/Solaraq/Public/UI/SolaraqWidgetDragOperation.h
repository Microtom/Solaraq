// SolaraqWidgetDragOperation.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "SolaraqWidgetDragOperation.generated.h"

class UUserWidget;

UCLASS()
class SOLARAQ_API USolaraqWidgetDragOperation : public UDragDropOperation
{
	GENERATED_BODY()
    
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Drag Operation")
	TObjectPtr<UUserWidget> WidgetReference;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Drag Operation")
	FVector2D DragOffset;

	// We override this to ensure the window reappears if the drag is cancelled/failed
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;
    
	// We override this to cleanup if needed on success
	virtual void Drop_Implementation(const FPointerEvent& PointerEvent) override;
};