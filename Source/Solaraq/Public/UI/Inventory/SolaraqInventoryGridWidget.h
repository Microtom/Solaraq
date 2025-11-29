// SolaraqInventoryGridWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/InventoryComponent.h"
#include "SolaraqInventoryGridWidget.generated.h"

class UCanvasPanel;
class USolaraqInventorySlotWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnGridDropEvent, const FPlacedItem&, DroppedItem, USolaraqInventoryGridWidget*, SourceGrid, USolaraqInventoryGridWidget*, TargetGrid, FIntPoint, TargetCoord);

UCLASS()
class SOLARAQ_API USolaraqInventoryGridWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Configuration ---
	UFUNCTION(BlueprintCallable, Category = "Solaraq|UI")
	void ConfigureGrid(int32 InWidth, int32 InHeight, float InSlotSize = 80.f);

	/** 
	 * Sets the Inventory Component this grid visually represents. 
	 * The grid does NOT listen to this component for updates (that's the Controller's job),
	 * but it holds the reference so Drag & Drop operations can validate moves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Solaraq|UI")
	void SetContextInventory(UInventoryComponent* InInventory);

	UFUNCTION(BlueprintCallable, Category = "Solaraq|UI")
	void UpdateState(const TArray<FPlacedItem>& InItems);

	UFUNCTION(BlueprintCallable, Category = "Solaraq|UI")
	void Redraw();

	// --- Getters ---
	UFUNCTION(BlueprintPure, Category = "Solaraq|UI")
	UInventoryComponent* GetContextInventory() const { return ContextInventory; }

	float GetSlotPixelSize() const { return SlotPixelSize; }
	UCanvasPanel* GetHighlightCanvas() const { return HighlightCanvas; }
	TSubclassOf<UUserWidget> GetHighlightWidgetClass() const { return HighlightWidgetClass; }

	// --- Events & Interaction ---
	UPROPERTY(BlueprintAssignable, Category = "Solaraq|UI")
	FOnGridDropEvent OnItemDrop;

	void SetItemToIgnore(const FGuid& ItemID);
	void ClearIgnoredItem();

protected:
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> SlotCanvas;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> ItemIconCanvas;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> HighlightCanvas;

	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|UI")
	TSubclassOf<USolaraqInventorySlotWidget> InventorySlotClass;

	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|UI")
	TSubclassOf<UUserWidget> ItemIconClass;

	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|UI")
	TSubclassOf<UUserWidget> HighlightWidgetClass;

private:
	// Defines visual dimensions
	int32 GridWidth = 0;
	int32 GridHeight = 0;
	float SlotPixelSize = 80.f;

	// Defines backend context (Weak reference logic)
	UPROPERTY()
	TObjectPtr<UInventoryComponent> ContextInventory;

	// Defines visual state
	TArray<FPlacedItem> CachedItems;
	FGuid ItemIDToIgnore;

	//Store references to the created background slots
	UPROPERTY()
	TArray<TObjectPtr<USolaraqInventorySlotWidget>> SlotWidgets;
};