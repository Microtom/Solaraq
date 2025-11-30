#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/InventoryComponent.h"
#include "SolaraqContainerWindowWidget.generated.h"

class USolaraqInventoryGridWidget;
class UButton;
class ASolaraqContainerBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnContainerWindowClosed);

UCLASS()
class SOLARAQ_API USolaraqContainerWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Initialize the window with the specific container data
	void InitContainerWindow(ASolaraqContainerBase* InContainerActor);

	UPROPERTY(BlueprintAssignable, Category = "Solaraq|Events")
	FOnContainerWindowClosed OnCloseRequested;

protected:
	virtual void NativeConstruct() override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	// --- Dragging Support ---
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	// ------------------------

	UFUNCTION()
	void HandleGridDrop(const FPlacedItem& DroppedItem, USolaraqInventoryGridWidget* SourceGrid, USolaraqInventoryGridWidget* TargetGrid, FIntPoint TargetCoord);

	UFUNCTION()
	void CloseWindow();

	// UI Elements
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USolaraqInventoryGridWidget> ContainerGrid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

private:
	UPROPERTY()
	TObjectPtr<ASolaraqContainerBase> LinkedContainer;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> ContainerInventory;
};