#include "Items/ItemActorBase.h"
#include "Pawns/SolaraqCharacterPawn.h"

AItemActorBase::AItemActorBase()
{
	PrimaryActorTick.bCanEverTick = false;
	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;
}

void AItemActorBase::SetOwningPawn(ASolaraqCharacterPawn* NewOwner)
{
	OwningPawn = NewOwner;
}

void AItemActorBase::OnEquip()
{
	// Base implementation does nothing. Child classes override this.
	// e.g., A weapon might play an equip sound here.
}

void AItemActorBase::OnUnequip()
{
	// Base implementation does nothing. Child classes override this.
	// e.g., A fishing rod might tell the FishingSubsystem to reset.
}

void AItemActorBase::PrimaryUse()
{
	// Base implementation does nothing.
}

void AItemActorBase::PrimaryUse_Stop()
{
	// Base implementation does nothing.
}

void AItemActorBase::SecondaryUse()
{
	// Base implementation does nothing.
}

void AItemActorBase::SecondaryUse_Stop()
{
	// Base implementation does nothing.
}