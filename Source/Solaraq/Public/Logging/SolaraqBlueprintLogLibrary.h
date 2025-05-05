// SolaraqBlueprintLogLibrary.h
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Logging/SolaraqLogChannels.h" // Include the header with our log categories and enums
#include "SolaraqBlueprintLogLibrary.generated.h" // Needs to be last include

// Forward declaration
enum class ESolaraqLogCategory : uint8;
enum class ESolaraqLogVerbosity : uint8;

UCLASS()
class SOLARAQ_API USolaraqBlueprintLogLibrary : public UBlueprintFunctionLibrary // Ensure SOLARAQ_API matches your project's API macro
{
	GENERATED_BODY()

public:
	/**
	 * Logs a message to a specific Solaraq log channel.
	 * @param WorldContextObject Provides the world context, necessary for PrintString. Get from 'self' in most Blueprints.
	 * @param Category The Solaraq log category to use.
	 * @param Verbosity The logging severity level (Log, Warning, Error etc).
	 * @param Message The string message to log.
	 * @param bPrintToScreen Should the message also be printed to the screen?
	 * @param ScreenMessageColor Color of the message if printed to screen.
	 * @param ScreenMessageDuration Duration the message stays on screen.
	 */
	UFUNCTION(BlueprintCallable, Category = "Solaraq|Logging", meta = (WorldContext = "WorldContextObject", Keywords = "log print debug solaraq", AdvancedDisplay = "bPrintToScreen,ScreenMessageColor,ScreenMessageDuration"))
	static void LogToSolaraqChannel(
		const UObject* WorldContextObject,
		ESolaraqLogCategory Category,
		ESolaraqLogVerbosity Verbosity,
		const FString& Message = FString(TEXT("Hello Solaraq!")),
		bool bPrintToScreen = false,
		FLinearColor ScreenMessageColor = FLinearColor::Green, // Default to green
		float ScreenMessageDuration = 3.f);                    // Default duration
};