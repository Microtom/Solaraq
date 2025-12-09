// UI/SolaraqMinimapInterface.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SolaraqMinimapInterface.generated.h"

UENUM(BlueprintType)
enum class EMinimapIconType : uint8
{
	Player,
	FriendlyShip,
	HostileShip,
	Planet,
	AsteroidField, // Use this for the texture look
	Station,
	Moon
};

USTRUCT(BlueprintType)
struct FSolaraqMinimapData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMinimapIconType IconType = EMinimapIconType::HostileShip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor IconColor = FLinearColor::White;

	// Use this for fixed icons (Ships, etc.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float IconScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsVisible = true;

	// --- NEW: Area Definitions ---

	// If true, the minimap will calculate the size of the icon based on World Radius vs Radar Range
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsArea = false;

	// The actual radius in World Units (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AreaRadius = 0.0f;
};

// ... Interface class remains the same ...
UINTERFACE(MinimalAPI)
class USolaraqMinimapInterface : public UInterface
{
	GENERATED_BODY()
};

class SOLARAQ_API ISolaraqMinimapInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Minimap")
	FSolaraqMinimapData GetMinimapData() const;
};