// SolaraqLogChannels.h
// -LogCmds="LogTemp VeryVerbose, LogBlueprint VeryVerbose" 
#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h" // Include for DECLARE_LOG_CATEGORY_EXTERN

// --- Declare Solaraq Log Categories ---
// Add more categories here as your game systems expand
DECLARE_LOG_CATEGORY_EXTERN(LogSolaraqGeneral, Log, All);    // For general messages
DECLARE_LOG_CATEGORY_EXTERN(LogSolaraqMovement, Log, All);   // For ship movement, physics, input
DECLARE_LOG_CATEGORY_EXTERN(LogSolaraqCombat, Log, All);     // For weapons, damage, projectiles
DECLARE_LOG_CATEGORY_EXTERN(LogSolaraqSystem, Log, All);     // For core game systems, components
DECLARE_LOG_CATEGORY_EXTERN(LogSolaraqAI, Log, All);         // For AI behavior
DECLARE_LOG_CATEGORY_EXTERN(LogSolaraqUI, Log, All);         // For UI related logs
DECLARE_LOG_CATEGORY_EXTERN(LogSolaraqCelestials, Log, All); // For planets and stars
DECLARE_LOG_CATEGORY_EXTERN(LogSolaraqProjectile, Log, All); // For planets and stars
DECLARE_LOG_CATEGORY_EXTERN(LogSolaraqMarker, Log, All); // For planets and stars
DECLARE_LOG_CATEGORY_EXTERN(LogSolaraqTurret, Log, All); // For planets and stars


// --- Blueprint Enum for Selecting Category ---
UENUM(BlueprintType)
enum class ESolaraqLogCategory : uint8
{
	General     UMETA(DisplayName = "General"),
	Movement    UMETA(DisplayName = "Movement"),
	Combat      UMETA(DisplayName = "Combat"),
	System      UMETA(DisplayName = "System"),
	AI          UMETA(DisplayName = "AI"),
	Celestials  UMETA(DisplayName = "Celestials"),
	UI          UMETA(DisplayName = "UI"),
	Projectile  UMETA(DisplayName = "Projectile"),
	Marker      UMETA(DisplayName = "Marker"),
	Turret      UMETA(DisplayName = "Turret")
	// Add matching entries here if you add new categories above
};

// --- Blueprint Enum for Selecting Verbosity ---
// Wrapper Enum for ELogVerbosity, exposed to Blueprints
UENUM(BlueprintType)
enum class ESolaraqLogVerbosity : uint8
{
	// We expose the common verbosity levels to BP
	Fatal       UMETA(DisplayName = "Fatal"), // Use sparingly! Crashes the game (usually).
	Error       UMETA(DisplayName = "Error"),
	Warning     UMETA(DisplayName = "Warning"),
	Display     UMETA(DisplayName = "Display"), // Logs to console and log file
	Log         UMETA(DisplayName = "Log"),     // Logs to log file only (by default)
	Verbose     UMETA(DisplayName = "Verbose"), // Detailed logs, hidden by default
	VeryVerbose UMETA(DisplayName = "Very Verbose") // Very detailed logs, hidden by default
};