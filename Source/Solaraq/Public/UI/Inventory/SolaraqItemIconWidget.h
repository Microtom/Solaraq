// SolaraqItemIconWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/InventoryComponent.h" 
#include "SolaraqItemIconWidget.generated.h"

class USolaraqInventoryGridWidget;
class UImage;
class UTextBlock;
class UButton; 

UCLASS()
class SOLARAQ_API USolaraqItemIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Initialize(const FPlacedItem& InItemInfo, USolaraqInventoryGridWidget* InOwningGrid);
	
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> QuantityText;
	
private:
	UPROPERTY()
	FPlacedItem ItemInfo;

	UPROPERTY()
	TObjectPtr<USolaraqInventoryGridWidget> OwningGrid;
};