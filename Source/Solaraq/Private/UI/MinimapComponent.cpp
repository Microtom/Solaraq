#include "UI/MinimapComponent.h" // Adjust path
#include "UI/MinimapTrackableComponent.h" // Adjust path
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h" // For logging

// DEFINE_LOG_CATEGORY_STATIC(LogMinimap, Log, All); // Optional: Custom log category

UMinimapComponent::UMinimapComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // Doesn't need to tick itself; the widget will pull data.
}

void UMinimapComponent::BeginPlay()
{
    Super::BeginPlay();

    // Create the minimap widget only on the owning client
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (PC && PC->IsLocalController())
    {
        if (MinimapWidgetClass)
        {
            MinimapWidgetInstance = CreateWidget<UUserWidget>(PC, MinimapWidgetClass);
            if (MinimapWidgetInstance)
            {
                MinimapWidgetInstance->AddToViewport();
                // UE_LOG(LogMinimap, Log, TEXT("Minimap widget created and added to viewport."));
            }
            else
            {
                // UE_LOG(LogMinimap, Error, TEXT("Failed to create minimap widget instance!"));
            }
        }
        else
        {
           // UE_LOG(LogMinimap, Warning, TEXT("MinimapWidgetClass is not set in MinimapComponent!"));
        }
    }
}

void UMinimapComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clean up widget if it exists
    if (MinimapWidgetInstance)
    {
        MinimapWidgetInstance->RemoveFromParent();
        MinimapWidgetInstance = nullptr;
    }

    // Clear tracked components (though they should unregister themselves)
    TrackedComponents.Empty();

    Super::EndPlay(EndPlayReason);
}

void UMinimapComponent::RegisterTrackable(UMinimapTrackableComponent* Trackable)
{
    if (Trackable)
    {
        TrackedComponents.AddUnique(Trackable); // AddUnique checks if it's already there
    }
}

void UMinimapComponent::UnregisterTrackable(UMinimapTrackableComponent* Trackable)
{
    if (Trackable)
    {
        TrackedComponents.Remove(Trackable);
    }
}

APawn* UMinimapComponent::GetPlayerPawn() const
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    return PC ? PC->GetPawn() : nullptr;
}

FVector UMinimapComponent::GetPlayerPawnLocationProjected() const
{
    APawn* PlayerPawn = GetPlayerPawn();
    if (PlayerPawn)
    {
        FVector Loc = PlayerPawn->GetActorLocation();
        Loc.Z = ProjectionZ; // Project onto the 2D plane
        return Loc;
    }
    return FVector::ZeroVector;
}

FRotator UMinimapComponent::GetPlayerPawnRotation() const
{
     APawn* PlayerPawn = GetPlayerPawn();
     return PlayerPawn ? PlayerPawn->GetActorRotation() : FRotator::ZeroRotator;
}