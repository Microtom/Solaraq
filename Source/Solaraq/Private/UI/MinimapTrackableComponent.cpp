#include "UI/MinimapTrackableComponent.h" 
#include "UI/MinimapComponent.h"         
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UMinimapTrackableComponent::UMinimapTrackableComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // Doesn't need to tick itself
}

void UMinimapTrackableComponent::BeginPlay()
{
    Super::BeginPlay();
    RegisterWithMinimapSystem();
}

void UMinimapTrackableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnregisterWithMinimapSystem();
    Super::EndPlay(EndPlayReason);
}

void UMinimapTrackableComponent::RegisterWithMinimapSystem()
{
    // Only register on clients or non-networked games where the local player exists
    if (GetOwner() && GetWorld() && GetWorld()->IsGameWorld() && !OwningMinimapComponent.IsValid())
    {
        // Find the local player controller
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0); // Assuming player index 0
        if (PC)
        {
            // Find the MinimapComponent on the Player Controller
            OwningMinimapComponent = PC->FindComponentByClass<UMinimapComponent>();
            if (OwningMinimapComponent.IsValid())
            {
                OwningMinimapComponent->RegisterTrackable(this);
                 // UE_LOG(LogTemp, Log, TEXT("Trackable '%s' registered with Minimap."), *GetOwner()->GetName());
            }
            else
            {
                 // UE_LOG(LogTemp, Warning, TEXT("Trackable '%s' could not find MinimapComponent on PlayerController."), *GetOwner()->GetName());
            }
        }
    }
}

void UMinimapTrackableComponent::UnregisterWithMinimapSystem()
{
    if (OwningMinimapComponent.IsValid())
    {
        OwningMinimapComponent->UnregisterTrackable(this);
        // UE_LOG(LogTemp, Log, TEXT("Trackable '%s' unregistered from Minimap."), *GetOwner()->GetName());
    }
    OwningMinimapComponent = nullptr; // Clear the weak ptr
}


FVector UMinimapTrackableComponent::GetTrackedActorLocation() const
{
    return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

FRotator UMinimapTrackableComponent::GetTrackedActorRotation() const
{
    return GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
}