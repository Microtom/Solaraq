// SolaraqBlueprintLogLibrary.cpp
#include "Logging/SolaraqBlueprintLogLibrary.h"
#include "Logging/SolaraqLogChannels.h"        // Include for our categories and enums
#include "Engine/Engine.h"                      // Include for GEngine (used by PrintString)
#include "Logging/LogVerbosity.h"               // Include for ELogVerbosity::Type and ToString(Verbosity)
#include "Logging/LogMacros.h"                  // Include for FMsg::Logf, UE_LOG
#include "Kismet/KismetSystemLibrary.h"         // Include for PrintString functionality

// Helper function to convert OUR Blueprint enum to the ENGINE's ELogVerbosity::Type
// Note: ELogVerbosity is defined in Logging/LogVerbosity.h
static ELogVerbosity::Type ConvertSolaraqVerbosity(ESolaraqLogVerbosity Verbosity)
{
    // Direct mapping since our enum values mirror the engine's common ones
    return static_cast<ELogVerbosity::Type>(Verbosity);

    // More explicit switch if you prefer or if enums diverge later:
    /*
    switch (Verbosity)
    {
        case ESolaraqLogVerbosity::Fatal:       return ELogVerbosity::Fatal;
        case ESolaraqLogVerbosity::Error:       return ELogVerbosity::Error;
        case ESolaraqLogVerbosity::Warning:     return ELogVerbosity::Warning;
        case ESolaraqLogVerbosity::Display:     return ELogVerbosity::Display;
        case ESolaraqLogVerbosity::Log:         return ELogVerbosity::Log;
        case ESolaraqLogVerbosity::Verbose:     return ELogVerbosity::Verbose;
        case ESolaraqLogVerbosity::VeryVerbose: return ELogVerbosity::VeryVerbose;
        default:                                return ELogVerbosity::Log; // Default fallback
    }
    */
}


void USolaraqBlueprintLogLibrary::LogToSolaraqChannel(
    const UObject* WorldContextObject,
    ESolaraqLogCategory Category,
    ESolaraqLogVerbosity Verbosity,
    const FString& Message,
    bool bPrintToScreen,
    FLinearColor ScreenMessageColor,
    float ScreenMessageDuration)
{
    // Determine the FName of the category based on the enum input
    FName LogCategoryName;
    switch (Category)
    {
        case ESolaraqLogCategory::General:
            LogCategoryName = LogSolaraqGeneral.GetCategoryName();
            break;
        case ESolaraqLogCategory::Movement:
            LogCategoryName = LogSolaraqMovement.GetCategoryName();
            break;
        case ESolaraqLogCategory::Combat:
            LogCategoryName = LogSolaraqCombat.GetCategoryName();
            break;
        case ESolaraqLogCategory::System:
            LogCategoryName = LogSolaraqSystem.GetCategoryName();
            break;
        case ESolaraqLogCategory::AI:
            LogCategoryName = LogSolaraqAI.GetCategoryName();
            break;
        case ESolaraqLogCategory::UI:
            LogCategoryName = LogSolaraqUI.GetCategoryName();
            break;
        case ESolaraqLogCategory::Celestials:
            LogCategoryName = LogSolaraqCelestials.GetCategoryName();
            break;
        case ESolaraqLogCategory::Projectile:
            LogCategoryName = LogSolaraqProjectile.GetCategoryName();
            break;
        case ESolaraqLogCategory::Marker:
            LogCategoryName = LogSolaraqMarker.GetCategoryName();
            break;
        case ESolaraqLogCategory::Turret:
            LogCategoryName = LogSolaraqTurret.GetCategoryName();
            break;
        case ESolaraqLogCategory::Transition:
            LogCategoryName = LogSolaraqTransition.GetCategoryName();
            break;
        // Add cases for new categories here
        default:
            // Fallback to General if an invalid enum value is somehow passed
            LogCategoryName = LogSolaraqGeneral.GetCategoryName();
            UE_LOG(LogSolaraqGeneral, Warning, TEXT("LogToSolaraqChannel: Invalid Category provided. Falling back to General. Message: %s"), *Message);
    }

    // Convert our Blueprint-friendly enum to the engine's required enum type
    ELogVerbosity::Type ActualVerbosity = ConvertSolaraqVerbosity(Verbosity);

    // Log using the core engine logging macro. This handles file/line info automatically.
    FMsg::Logf(UE_LOG_SOURCE_FILE(__FILE__), __LINE__, LogCategoryName, ActualVerbosity, TEXT("%s"), *Message);

    // --- Optional: Print to screen ---
    // Check if GEngine is valid (it might not be during shutdown or certain contexts)
    // Also check if WorldContextObject is valid, as PrintString needs it.
    if (bPrintToScreen && GEngine && WorldContextObject)
    {
        // Use the engine verbosity type for the string representation (ToString is in LogVerbosity.h)
        FString Prefix = FString::Printf(TEXT("[%s][%s] "), *LogCategoryName.ToString(), ToString(ActualVerbosity));
        FString FinalMessage = Prefix + Message;

        // UKismetSystemLibrary::PrintString requires a non-const UObject*.
        // We use const_cast here, assuming the WorldContextObject won't actually be modified by PrintString.
        // This is a common pattern when interfacing with older APIs.
        UObject* MutableWorldContext = const_cast<UObject*>(WorldContextObject);
        UKismetSystemLibrary::PrintString(MutableWorldContext, FinalMessage, true, true, ScreenMessageColor, ScreenMessageDuration);
    }
}