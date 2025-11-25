// UI/SolaraqMinimapIcon.cpp
#include "UI/SolaraqMinimapIcon.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void USolaraqMinimapIcon::SetupIcon(EMinimapIconType IconType, FLinearColor IconColor, float Scale)
{
	if (!MainImage) return;

	// 1. Set Color and Texture (Existing logic)
	MainImage->SetColorAndOpacity(IconColor);
	if (TObjectPtr<UTexture2D>* FoundTexture = IconTextures.Find(IconType))
	{
		if (*FoundTexture) MainImage->SetBrushFromTexture(*FoundTexture);
	}

	// 2. Set Scale
	// We modify the Widget's Render Transform directly. 
	// This scales the image from its Pivot Point (which should be 0.5, 0.5)
	SetRenderScale(FVector2D(Scale, Scale));
}