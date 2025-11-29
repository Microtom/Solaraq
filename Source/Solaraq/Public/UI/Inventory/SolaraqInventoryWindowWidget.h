// SolaraqInventoryWindowWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/InventoryComponent.h"
#include "SolaraqInventoryWindowWidget.generated.h"

class USolaraqInventoryGridWidget;
class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSolaraqInventoryClose);

UCLASS()
class SOLARAQ_API USolaraqInventoryWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintAssignable, Category = "Solaraq|Events")
	FOnSolaraqInventoryClose OnCloseRequested;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
	UFUNCTION()
	void RefreshWindow();

	UFUNCTION()
	void HandleGridDrop(const FPlacedItem& DroppedItem, USolaraqInventoryGridWidget* SourceGrid, USolaraqInventoryGridWidget* TargetGrid, FIntPoint TargetCoord);

	UFUNCTION()
	void CloseWindow();
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USolaraqInventoryGridWidget> BackpackGrid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;
	
private:
	UPROPERTY()
	TObjectPtr<UInventoryComponent> InventoryComp;
};