#include "Actors/Interactables/SolaraqInteractableChair.h" // Adjust path
#include "Pawns/SolaraqCharacterPawn.h"
#include "Controllers/SolaraqCharacterPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"

ASolaraqInteractableChair::ASolaraqInteractableChair()
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

FVector ASolaraqInteractableChair::GetEntryPointLocation() const
{
    return EntryPoint->GetComponentLocation();
}

void ASolaraqInteractableChair::Interact_Implementation(APawn* InteractingPawn)
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

    // Case 2: Chair is occupied -> Do nothing
    if (SeatedPawn != nullptr) return;

    // --- DISTANCE CHECK ---
    FVector CharLoc = SolaraqChar->GetActorLocation();
    FVector EntryLoc = EntryPoint->GetComponentLocation();
    CharLoc.Z = 0; 
    EntryLoc.Z = 0;

    float DistanceToSeat = FVector::Dist(CharLoc, EntryLoc);

    // Same tolerances as before
    if (DistanceToSeat > 60.0f) 
    {
        if (ASolaraqCharacterPlayerController* PC = Cast<ASolaraqCharacterPlayerController>(SolaraqChar->GetController()))
        {
            PC->RequestMoveToInteract(this, EntryPoint->GetComponentLocation(), 50.0f);
        }
    }
    else
    {
        Sit(SolaraqChar);
    }
}

void ASolaraqInteractableChair::Sit(ASolaraqCharacterPawn* PawnToSit)
{
    if (!PawnToSit || SeatedPawn != nullptr) return;

    SeatedPawn = PawnToSit;
    
    // CHANGED: Call the new Sequence function
    if (SeatAttachmentPoint && EntryPoint)
    {
        PawnToSit->BeginSittingSequence(EntryPoint, SeatAttachmentPoint);
    }
}