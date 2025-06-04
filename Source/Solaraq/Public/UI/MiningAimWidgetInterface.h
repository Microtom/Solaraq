// MiningAimWidgetInterface.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MiningAimWidgetInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UMiningAimWidgetInterface : public UInterface
{
	GENERATED_BODY()
};

class SOLARAQ_API IMiningAimWidgetInterface
{
	GENERATED_BODY()

public:
	/**
	 * Called to update the aiming indicator's rotation on screen.
	 * @param ScreenAngleDegrees The desired angle in degrees for the widget's visual indicator.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Mining Aim Widget")
	void SetAimDirection(float ScreenAngleDegrees);
};