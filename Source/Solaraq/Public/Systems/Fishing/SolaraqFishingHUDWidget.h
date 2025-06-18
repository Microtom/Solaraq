// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SolaraqFishingHUDWidget.generated.h"

class UProgressBar;
/**
 * 
 */
UCLASS()
class SOLARAQ_API USolaraqFishingHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void UpdateTension(float NewTensionPercent);
	
protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> TensionProgressBar;
	
};
