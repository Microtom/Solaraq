// ItemActorBase.cpp
#include "Items/ItemActorBase.h"
#include "Pawns/SolaraqCharacterPawn.h"

AItemActorBase::AItemActorBase()
{
	PrimaryActorTick.bCanEverTick = false;
	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;
}

void AItemActorBase::OnEquip(ASolaraqCharacterPawn* NewOwner)
{
	OwningPawn = NewOwner;
	SetOwner(NewOwner); // Set the actor-level owner
}

void AItemActorBase::OnUnequip()
{
	OwningPawn = nullptr;
	SetOwner(nullptr);
}

void AItemActorBase::PrimaryUse()
{
	// Base implementation does nothing. Overridden in child classes (e.g., Weapon fires, Fishing Rod casts line)
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