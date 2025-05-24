// Components/DockingPadComponent.cpp

#include "Components/DockingPadComponent.h"
#include "Components/BoxComponent.h"
#include "Pawns/SolaraqShipBase.h" // Include the ship base
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Logging/SolaraqLogChannels.h" // Your custom log channels

UDockingPadComponent::UDockingPadComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // No tick needed for now

	DockingTriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("DockingTriggerVolume"));
	DockingTriggerVolume->SetupAttachment(this); // Attach to this SceneComponent
	DockingTriggerVolume->SetBoxExtent(FVector(250.0f, 250.0f, 100.0f)); // Decent default size
	DockingTriggerVolume->SetCollisionProfileName(TEXT("Trigger"));    // Overlap only
	DockingTriggerVolume->SetGenerateOverlapEvents(true);
	DockingTriggerVolume->SetHiddenInGame(false); // Crucial: Make the box visible in game
	DockingTriggerVolume->SetMobility(EComponentMobility::Movable); // So it can be on moving stations

	// Optional: Assign a simple material for better visibility if needed
	// static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("Material'/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial'"));
	// if (MaterialFinder.Succeeded()) { DockingTriggerVolume->SetMaterial(0, MaterialFinder.Object); }

	OccupyingShip_Server = nullptr;
}

void UDockingPadComponent::BeginPlay()
{
	Super::BeginPlay();

	// --- LOGGING FOR PAD INITIALIZATION ---
	AActor* MyOwner = GetOwner();
	FString OwnerName = MyOwner ? MyOwner->GetName() : TEXT("UNKNOWN_OWNER");
	FString WorldName = GetWorld() ? GetWorld()->GetName() : TEXT("UNKNOWN_WORLD");
	float CurrentTime = GetWorld() ? GetWorld()->TimeSeconds : -1.f;

	UE_LOG(LogSolaraqTransition, Warning, TEXT("DockingPadComponent BEGINPLAY: Name: %s, Owner: %s, UniqueID: %s, World: %s, Time: %.2f"),
		*GetName(),
		*OwnerName,
		*DockingPadUniqueID.ToString(), // Make sure DockingPadUniqueID is set correctly in BP/Editor
		*WorldName,
		CurrentTime);
	// --- END LOGGING ---
	
	// Bind overlap events only on the server. The server will manage docking state.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		DockingTriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &UDockingPadComponent::OnDockingVolumeBeginOverlap);
		DockingTriggerVolume->OnComponentEndOverlap.AddDynamic(this, &UDockingPadComponent::OnDockingVolumeEndOverlap);
	}
}

void UDockingPadComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UDockingPadComponent::OnDockingVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Server-side logic only
	if (!GetOwner() || !GetOwner()->HasAuthority() || !OtherActor)
	{
		return;
	}

	ASolaraqShipBase* Ship = Cast<ASolaraqShipBase>(OtherActor);
	if (Ship && !Ship->IsShipDockedOrDocking() && IsPadFree_Server())
	{
		UE_LOG(LogSolaraqSystem, Log, TEXT("DockingPad %s: Ship %s entered trigger. Requesting dock."), *GetName(), *Ship->GetName());
		// The ship initiates the dock request to itself (which is a server call)
		Ship->Server_RequestDockWithPad(this);
	}
}

void UDockingPadComponent::OnDockingVolumeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// Server-side logic only
	if (!GetOwner() || !GetOwner()->HasAuthority() || !OtherActor)
	{
		return;
	}
    
	ASolaraqShipBase* Ship = Cast<ASolaraqShipBase>(OtherActor);
	// If the ship that left is the one currently occupying this pad (or was attempting to)
    // and the ship BELIEVES it's docked to THIS pad.
	if (Ship && OccupyingShip_Server == Ship && Ship->GetActiveDockingPad() == this)
	{
        // For now, undocking is explicit via player/AI action.
        // Leaving the volume while docked doesn't automatically undock,
        // but we could add that logic here if desired.
		UE_LOG(LogSolaraqSystem, Log, TEXT("DockingPad %s: Docked Ship %s left trigger. Consider manual undock."), *GetName(), *Ship->GetName());
        // Example: Ship->Server_RequestUndock(); // If auto-undock on leaving volume is desired
	}
}

void UDockingPadComponent::SetOccupyingShip_Server(ASolaraqShipBase* Ship)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		OccupyingShip_Server = Ship;
		UE_LOG(LogSolaraqSystem, Log, TEXT("DockingPad %s: Now occupied by %s."), *GetName(), *GetNameSafe(Ship));
	}
}

void UDockingPadComponent::ClearOccupyingShip_Server()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		UE_LOG(LogSolaraqSystem, Log, TEXT("DockingPad %s: Cleared occupying ship %s."), *GetName(), *GetNameSafe(OccupyingShip_Server));
		OccupyingShip_Server = nullptr;
	}
}

bool UDockingPadComponent::IsPadFree_Server() const
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return OccupyingShip_Server == nullptr;
	}
	return false; // Clients should not rely on this
}

ASolaraqShipBase* UDockingPadComponent::GetOccupyingShip_Server() const
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return OccupyingShip_Server;
	}
	// As with IsPadFree_Server, clients should not rely on this for authoritative state.
	// Returning nullptr for clients.
	UE_LOG(LogSolaraqSystem, Verbose, TEXT("DockingPadComponent %s: GetOccupyingShip_Server called by non-authority or no owner. Returning nullptr."), *GetName());
	return nullptr;
}
