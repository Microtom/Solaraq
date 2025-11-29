#include "Actors/Interactables/InteractableChair.h"
#include "Pawns/SolaraqCharacterPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Controllers/SolaraqCharacterPlayerController.h"

AInteractableChair::AInteractableChair()
{
    PrimaryActorTick.bCanEverTick = false;

    DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
    RootComponent = DefaultSceneRoot;

    ChairMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairMesh"));
    ChairMesh->SetupAttachment(RootComponent);

    SeatAttachmentPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SeatAttachmentPoint"));
    SeatAttachmentPoint->SetupAttachment(ChairMesh);
    // Move seat up slightly so they aren't inside the mesh
    SeatAttachmentPoint->SetRelativeLocation(FVector(0.f, 0.f, 50.f)); 

    EntryPoint = CreateDefaultSubobject<USceneComponent>(TEXT("EntryPoint"));
    EntryPoint->SetupAttachment(RootComponent);
    // Default entry point 100 units in front of the chair
    EntryPoint->SetRelativeLocation(FVector(100.f, 0.f, 0.f)); 
    
    SeatedPawn = nullptr;
}

FVector AInteractableChair::GetEntryPointLocation() const
{
    return EntryPoint->GetComponentLocation();
}

void AInteractableChair::Interact_Implementation(APawn* InteractingPawn)
{
    // If someone is already sitting here, and it's NOT the person trying to interact, ignore.
    if (SeatedPawn && SeatedPawn != InteractingPawn)
    {
        return;
    }

    // If the person interacting is ALREADY the one sitting, they want to stand up.
    if (SeatedPawn == InteractingPawn)
    {
        ASolaraqCharacterPawn* SolaraqChar = Cast<ASolaraqCharacterPawn>(InteractingPawn);
        if (SolaraqChar)
        {
            SolaraqChar->StandUp();
            SeatedPawn = nullptr;
        }
        return;
    }

    // --- LOGIC START: Move to Chair ---
    // Instead of sitting immediately, we tell the Controller to move us there first.
    if (ASolaraqCharacterPlayerController* PC = Cast<ASolaraqCharacterPlayerController>(InteractingPawn->GetController()))
    {
        // This triggers the "Auto-Pilot" in the controller
        PC->MoveToAndInteract(this);
    }
}

void AInteractableChair::Sit(ASolaraqCharacterPawn* PawnToSit)
{
    if (!PawnToSit || SeatedPawn != nullptr) return;

    SeatedPawn = PawnToSit;
    
    // Call the Pawn's sitting logic
    PawnToSit->SitDown(SeatAttachmentPoint);
}