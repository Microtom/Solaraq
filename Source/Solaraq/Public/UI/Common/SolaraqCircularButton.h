	// SolaraqCircularButton.h
#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "SolaraqCircularButton.generated.h"

/**
 * A custom button that only registers clicks within a circular radius 
 * inscribed in the widget's box, ignoring the transparent corners.
 */
UCLASS()
class SOLARAQ_API USolaraqCircularButton : public UButton
{
	GENERATED_BODY()

public:
	// Override the underlying Slate widget creation to use our custom SButton
	virtual TSharedRef<SWidget> RebuildWidget() override;
};