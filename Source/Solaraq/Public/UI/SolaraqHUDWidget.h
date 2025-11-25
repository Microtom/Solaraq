// SolaraqHUDWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SolaraqHUDWidget.generated.h"

class UCanvasPanel;
class UDragDropOperation;
struct FGeometry;
class FDragDropEvent;
class USolaraqMinimapWidget;

UCLASS()
class SOLARAQ_API USolaraqHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCanvasPanel* GetMainCanvas();

	USolaraqMinimapWidget* GetMinimapWidget() const { return WBP_Minimap; }
	
protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> MainCanvas;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USolaraqMinimapWidget> WBP_Minimap; 
	
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
};