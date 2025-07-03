// SolaraqInventoryWindowWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SolaraqInventoryWindowWidget.generated.h"

// Forward Declarations
class UBorder;
class UTextBlock;
class USolaraqInventoryGridWidget;

UCLASS()
class SOLARAQ_API USolaraqInventoryWindowWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeSupportsKeyboardFocus() const override;
	//~ End UUserWidget Interface

public:
	void SetMoneyText(int32 Amount);
	
protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Header_Border;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MoneyText_Block;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USolaraqInventoryGridWidget> WBP_InventoryGrid;

private:
	// We are back to needing the DragOffset.
	FVector2D DragOffset;
	
	bool bIsDragging = false;
};