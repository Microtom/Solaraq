// UI/SolaraqMinimapIcon.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SolaraqMinimapInterface.h"
#include "SolaraqMinimapIcon.generated.h"

class UImage;
class UTexture2D;

UCLASS()
class SOLARAQ_API USolaraqMinimapIcon : public UUserWidget
{
	GENERATED_BODY()

public:
	// Standard C++ function, no Blueprint logic needed
	void SetupIcon(EMinimapIconType IconType, FLinearColor IconColor, float Scale);

protected:
	// This connects the C++ variable to the Image widget in the Designer.
	// IMPORTANT: You MUST name the Image widget "MainImage" in the Blueprint.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> MainImage;

	// Configuration: Assign textures to Enums in the Editor
	UPROPERTY(EditDefaultsOnly, Category = "Icon Config")
	TMap<EMinimapIconType, TObjectPtr<UTexture2D>> IconTextures;
};