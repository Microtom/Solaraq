#include "Actors/Interactables/SolaraqContainerBase.h"
#include "Controllers/SolaraqCharacterPlayerController.h"
#include "Logging/SolaraqLogChannels.h"

ASolaraqContainerBase::ASolaraqContainerBase()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	LidMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidMeshComponent"));
	LidMeshComponent->SetupAttachment(MeshComponent);

	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	InventoryComponent->SetIsReplicated(true); 
}

void ASolaraqContainerBase::BeginPlay()
{
	Super::BeginPlay();
	ClosedLidRotation = LidMeshComponent->GetRelativeRotation();
}

FVector ASolaraqContainerBase::GetInteractionLocation() const
{
	// Return a point slightly in front of the chest (based on Forward Vector)
	return GetActorLocation() + (GetActorForwardVector() * 100.0f);
}

void ASolaraqContainerBase::Interact_Implementation(APawn* InteractingPawn)
{
	UE_LOG(LogSolaraqSystem, Log, TEXT("Container %s: Interact called by %s"), *GetName(), *GetNameSafe(InteractingPawn));

	if (!InteractingPawn) return;

	ASolaraqCharacterPlayerController* PC = Cast<ASolaraqCharacterPlayerController>(InteractingPawn->GetController());
	if (!PC)
	{
		UE_LOG(LogSolaraqSystem, Warning, TEXT("Container interaction failed: Pawn has no SolaraqCharacterPlayerController."));
		return;
	}

	// 1. Check Distance
	// Ignore Z for checking horizontal distance (cylinder check)
	FVector MyLoc = GetInteractionLocation();
	FVector PawnLoc = InteractingPawn->GetActorLocation();
	MyLoc.Z = PawnLoc.Z; 
	
	float DistSq = FVector::DistSquared(MyLoc, PawnLoc);
	// Increase AcceptanceRadius so it triggers reliably when navigation stops
	float AcceptanceRadius = 180.0f; 

	if (DistSq > AcceptanceRadius * AcceptanceRadius)
	{
		UE_LOG(LogSolaraqSystem, Log, TEXT("Container too far (Dist: %.2f). Requesting move to %s"), FMath::Sqrt(DistSq), *GetInteractionLocation().ToString());
		// We tell PC to move, and use a radius slightly smaller than our check here to ensure we enter the zone
		PC->RequestMoveToInteract(this, GetInteractionLocation(), 120.0f);
	}
	else
	{
		UE_LOG(LogSolaraqSystem, Log, TEXT("Container in range. Opening UI."));
		
		// Stop any auto-movement
		PC->StopMovement();
		
		// Call function on Controller to create the widget
		PC->OpenContainerInventory(this);
	}
}

void ASolaraqContainerBase::OpenContainer(APlayerController* PlayerController)
{
	if (bIsOpen) return;
	bIsOpen = true;

	LidMeshComponent->SetRelativeRotation(OpenLidRotation);

	UE_LOG(LogTemp, Log, TEXT("Container Opened."));
}

void ASolaraqContainerBase::CloseContainer()
{
	if (!bIsOpen) return;
	bIsOpen = false;

	LidMeshComponent->SetRelativeRotation(ClosedLidRotation);
	UE_LOG(LogTemp, Log, TEXT("Container Closed."));
}