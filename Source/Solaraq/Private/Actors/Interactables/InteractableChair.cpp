#include "Actors/Interactables/InteractableChair.h" // Adjust this path if your folder structure is different
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

    SeatAttachmentPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SeatAttachmentPoint"));
    SeatAttachmentPoint->SetupAttachment(ChairMesh);
    
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
        // The logic assumes the Character class has EndInteraction/BeginInteraction.
        // If these don't exist yet, comment them out to compile.
        // Character->EndInteraction(); 
        
        Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        
        // Give them a little space so they don't get stuck inside the chair mesh
        const FVector ExitLocation = Character->GetActorForwardVector() * 100.f;
        Character->AddActorWorldOffset(ExitLocation, false, nullptr, ETeleportType::TeleportPhysics);

        // Re-enable collision or movement here if you disabled it during sitting
        if (Character->GetCharacterMovement())
        {
            Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        }

        SeatedPawn = nullptr;
    }
    // --- Logic for SITTING IN the chair ---
    else if (SeatedPawn == nullptr)
    {
        SeatedPawn = Character;
        // Character->BeginInteraction(this); 

        // Attach the pawn to our seat socket.
        Character->AttachToComponent(SeatAttachmentPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        
        // Zero out location/rotation relative to the seat point
        Character->SetActorRelativeLocation(FVector::ZeroVector);
        Character->SetActorRelativeRotation(FRotator::ZeroRotator);

        // Disable movement while seated so they don't walk away with the chair attached
        if (Character->GetCharacterMovement())
        {
            Character->GetCharacterMovement()->DisableMovement();
        }
    }
}