// SolaraqLogChannels.cpp
#include "Logging/SolaraqLogChannels.h" // Include the header file we just created

// --- Define Solaraq Log Categories ---
// Match the names used in DECLARE_LOG_CATEGORY_EXTERN in the .h file
DEFINE_LOG_CATEGORY(LogSolaraqGeneral);
DEFINE_LOG_CATEGORY(LogSolaraqMovement);
DEFINE_LOG_CATEGORY(LogSolaraqCombat);
DEFINE_LOG_CATEGORY(LogSolaraqSystem);
DEFINE_LOG_CATEGORY(LogSolaraqAI);
DEFINE_LOG_CATEGORY(LogSolaraqUI);
DEFINE_LOG_CATEGORY(LogSolaraqCelestials);
DEFINE_LOG_CATEGORY(LogSolaraqProjectile);
DEFINE_LOG_CATEGORY(LogSolaraqMarker);
DEFINE_LOG_CATEGORY(LogSolaraqTurret);