// Fill out your copyright notice in the Description page of Project Settings.


#include "Systems/Fishing/SolaraqFishingHUDWidget.h"
#include "Components/ProgressBar.h"

void USolaraqFishingHUDWidget::UpdateTension(float NewTensionPercent)
{
	if (TensionProgressBar) // Always good to check if it was bound successfully
	{
		TensionProgressBar->SetPercent(NewTensionPercent);

		// Now we can add our new feature!
		if (NewTensionPercent > 0.9f)
		{
			// Make it pulse or turn red
			TensionProgressBar->SetFillColorAndOpacity(FLinearColor::Red);
		}
		else
		{
			// Return to normal color
			TensionProgressBar->SetFillColorAndOpacity(FLinearColor::White); // Or whatever the default is
		}
	}
}
