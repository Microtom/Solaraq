#include "Actors/Interactables/InteractableChair.h" // Adjust path
#include "Pawns/SolaraqCharacterPawn.h"
#include "Controllers/SolaraqCharacterPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"

AInteractableChair::AInteractableChair()
{
    PrimaryActorTick.bCanEverTick = false;

    DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
    RootComponent = DefaultSceneRoot;

    ChairMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairMesh"));
    ChairMesh->SetupAttachment(RootComponent);

    SeatAttachmentPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SeatAttachmentPoint"));
    SeatAttachmentPoint->SetupAttachment(ChairMesh);
    SeatAttachmentPoint->SetRelativeLocation(FVector(0.f, 0.f, 50.f)); // Slight offset up

    EntryPoint = CreateDefaultSubobject<USceneComponent>(TEXT("EntryPoint"));
    EntryPoint->SetupAttachment(RootComponent);
    EntryPoint->SetRelativeLocation(FVector(100.f, 0.f, 0.f)); // 1m in front of chair
    
    SeatedPawn = nullptr;
}

FVector AInteractableChair::GetEntryPointLocation() const
{
    return EntryPoint->GetComponentLocation();
}

void AInteractableChair::Interact_Implementation(APawn* InteractingPawn)
{
    ASolaraqCharacterPawn* SolaraqChar = Cast<ASolaraqCharacterPawn>(InteractingPawn);
    if (!SolaraqChar) return;

    // Case 1: Player is ALREADY sitting -> Stand up
    if (SeatedPawn == SolaraqChar)
    {
        SolaraqChar->StandUp();
        SeatedPawn = nullptr;
        return;
    }

    // Case 2: Someone else is sitting -> Do nothing
    if (SeatedPawn != nullptr) return;

    // --- DISTANCE CHECK ---
    float DistanceToSeat = FVector::Dist(SolaraqChar->GetActorLocation(), EntryPoint->GetComponentLocation());
    
    // We allow a small tolerance (e.g., 15 units)
    if (DistanceToSeat > 15.0f) 
    {
        // Case 3: Too far away -> Request Controller to walk us here
        if (ASolaraqCharacterPlayerController* PC = Cast<ASolaraqCharacterPlayerController>(SolaraqChar->GetController()))
        {
            // The Controller will walk the pawn here, and then CALL THIS FUNCTION AGAIN.
            PC->RequestMoveToInteract(this, EntryPoint->GetComponentLocation());
        }
    }
    else
    {
        // Case 4: We are close enough -> SIT
        Sit(SolaraqChar);
    }
}

void AInteractableChair::Sit(ASolaraqCharacterPawn* PawnToSit)
{
    if (!PawnToSit || SeatedPawn != nullptr) return;

    SeatedPawn = PawnToSit;
    
    // Perform the physical attach logic on the pawn
    PawnToSit->SitDown(SeatAttachmentPoint);
}