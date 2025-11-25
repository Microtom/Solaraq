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
	AsteroidField,
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float IconScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsVisible = true;
};

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