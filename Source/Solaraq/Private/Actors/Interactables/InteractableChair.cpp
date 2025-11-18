// InteractableChair.cpp
#include "Actors/Interactables/InteractableChair.h" // Adjust path as needed
#include "Pawns/SolaraqCharacterPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AInteractableChair::AInteractableChair()
{
    PrimaryActorTick.bCanEverTick = false;

    DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
    RootComponent = DefaultSceneRoot;

    ChairMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairMesh"));
    ChairMesh->SetupAttachment(RootComponent);
    // In the Blueprint version of this class, you will set the actual static mesh for the chair.

    SeatAttachmentPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SeatAttachmentPoint"));
    SeatAttachmentPoint->SetupAttachment(ChairMesh);
    // In the Blueprint, you will move this component to the correct seating position.
    
    SeatedPawn = nullptr;
}

void AInteractableChair::Interact_Implementation(APawn* InteractingPawn)
{
    ASolaraqCharacterPawn* Character = Cast<ASolaraqCharacterPawn>(InteractingPawn);
    if (!Character)
    {
        return; // Not the right kind of pawn
    }

    // --- Logic for GETTING OUT of the chair ---
    if (SeatedPawn == Character)
    {
        // This pawn is already seated, so interacting again means they want to get up.
        Character->EndInteraction(); // Tell the pawn it's free now
        Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        
        // Give them a little space so they don't get stuck in the chair
        Character->AddActorWorldOffset(Character->GetActorForwardVector() * 50.f, false, nullptr, ETeleportType::TeleportPhysics);

        SeatedPawn = nullptr;
    }
    // --- Logic for SITTING IN the chair ---
    else if (SeatedPawn == nullptr)
    {
        // The chair is empty, so let this pawn sit down.
        SeatedPawn = Character;
        Character->BeginInteraction(this); // Tell the pawn it's now interacting

        // Attach the pawn to our seat socket.
        Character->AttachToComponent(SeatAttachmentPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        Character->SetActorRelativeLocation(FVector::ZeroVector);
        Character->SetActorRelativeRotation(FRotator::ZeroRotator);
    }
    // else, the chair is occupied by someone else, so do nothing.
}