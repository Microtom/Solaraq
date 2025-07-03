// SolaraqHUDWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SolaraqHUDWidget.generated.h"

class UCanvasPanel;
class UDragDropOperation;
struct FGeometry;
class FDragDropEvent;

UCLASS()
class SOLARAQ_API USolaraqHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCanvasPanel* GetMainCanvas();

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> MainCanvas;

	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
};