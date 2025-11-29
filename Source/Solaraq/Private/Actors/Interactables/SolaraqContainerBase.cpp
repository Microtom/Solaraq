#include "Actors/Interactables/SolaraqContainerBase.h"

ASolaraqContainerBase::ASolaraqContainerBase()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	LidMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidMeshComponent"));
	LidMeshComponent->SetupAttachment(MeshComponent);

	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	InventoryComponent->SetIsReplicated(true); // If multiplayer
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

void ASolaraqContainerBase::OpenContainer(APlayerController* PlayerController)
{
	if (bIsOpen) return;
	bIsOpen = true;

	// Simple Instant Animation (Use a Timeline here for smoothness in the future)
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