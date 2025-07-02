// SolaraqInventoryWindowWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SolaraqInventoryWindowWidget.generated.h"

// Forward Declarations
class UBorder;
class UTextBlock;
class USolaraqInventoryGridWidget;

/**
 * The main window frame for displaying an inventory. Handles dragging and other window-like behaviors.
 */
UCLASS()
class SOLARAQ_API USolaraqInventoryWindowWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	//~ End UUserWidget Interface

public:
	// This function will be called from blueprints or C++ to update the money display
	UFUNCTION(BlueprintCallable, Category = "Inventory Window")
	void SetMoneyText(int32 Amount);
	
protected:
	// --- WIDGET BINDINGS ---
	// We bind these UPROPERTYs to the widgets in the WBP_InventoryWindow blueprint.
	// The `meta = (BindWidget)` tag is what allows the blueprint to see and link this property.

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Header_Border; // The draggable header area

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MoneyText_Block; // The text block in the footer for money

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USolaraqInventoryGridWidget> WBP_InventoryGrid; // A direct reference to our grid

private:
	// --- DRAG AND DROP LOGIC ---

	// True if we are currently dragging the window
	bool bIsDragging = false;
	
	// The offset from the mouse cursor to the top-left corner of the widget when dragging starts
	FVector2D DragOffset;
};