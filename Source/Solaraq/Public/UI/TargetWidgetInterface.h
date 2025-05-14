// TargetWidgetInterface.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TargetWidgetInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, Blueprintable) // Blueprintable allows BPs to implement this C++ interface
class UTargetWidgetInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for widgets that can display a locked state (e.g., target markers).
 */
class SOLARAQ_API ITargetWidgetInterface // Replace YOURPROJECT_API with your project's actual API macro (e.g., SOLARAQ_API)
{
    GENERATED_BODY()

public:
    // Function to be implemented in Blueprints (or C++) to update the widget's appearance based on lock state.
    // This is a BlueprintImplementableEvent, meaning its logic is expected to be defined in the Blueprint that implements this interface.
    UFUNCTION(BlueprintImplementableEvent, Category = "Target Widget")
    void SetLockedState(bool bIsCurrentlyLocked);
};