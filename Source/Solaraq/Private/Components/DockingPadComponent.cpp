// DockingPadComponent.cpp

#include "Components/DockingPadComponent.h"
#include "Components/PrimitiveComponent.h" // For the volume
#include "Components/SceneComponent.h"     // For the attach point
#include "Pawns/SolaraqShipBase.h" // <<< ENSURE THIS IS PRESENT
#include "Components/SphereComponent.h" 
#include "Components/BoxComponent.h"      // To check ship's root comp type
#include "GameFramework/Actor.h"           // For GetOwner()
#include "Net/UnrealNetwork.h"
#include "Logging/SolaraqLogChannels.h"    // Optional logging

UDockingPadComponent::UDockingPadComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // No tick needed for this component

    // Defaults - Note: DockingVolume and AttachPoint MUST be assigned in the owning Actor/Blueprint
    DockingVolume = nullptr;
    AttachPoint = nullptr;
    bAutoDockOnEnter = true;
    CurrentDockingStatus = EDockingStatus::Available;
    CurrentDockedShip = nullptr;

    SetIsReplicatedByDefault(true); // Ensure component replicates
}

void UDockingPadComponent::BeginPlay()
{
    Super::BeginPlay();

    // --- Crucial Setup Validation and Binding ---
    // Only bind overlaps on the server, as server manages docking state
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (!IsValid(DockingVolume))
        {
            UE_LOG(LogSolaraqSystem, Error, TEXT("DockingPadComponent on %s: DockingVolume is NOT assigned! Docking will not function."), *GetOwner()->GetName());
            return; // Cannot proceed without a volume
        }
        if (!IsValid(AttachPoint))
        {
             UE_LOG(LogSolaraqSystem, Warning, TEXT("DockingPadComponent on %s: AttachPoint is NOT assigned! Ships will attach to pad owner's root."), *GetOwner()->GetName());
             // Can potentially continue, but attachment might look wrong
        }

        // Bind the overlap events
        DockingVolume->OnComponentBeginOverlap.AddDynamic(this, &UDockingPadComponent::OnDockingVolumeOverlapBegin);
        DockingVolume->OnComponentEndOverlap.AddDynamic(this, &UDockingPadComponent::OnDockingVolumeOverlapEnd);

         UE_LOG(LogSolaraqSystem, Log, TEXT("DockingPadComponent on %s initialized on Server."), *GetOwner()->GetName());
    }
    // Clients will rely on replication
}

void UDockingPadComponent::OnDockingVolumeOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Server-only logic for initiating docking
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    ASolaraqShipBase* Ship = Cast<ASolaraqShipBase>(OtherActor);
    // Check if it's a valid ship AND if the overlapping component is its physics root
    if (Ship && OtherComp == Ship->GetCollisionAndPhysicsRoot())
    {
         UE_LOG(LogSolaraqSystem, Log, TEXT("Docking Pad %s: Ship %s entered volume."), *GetNameSafe(this), *Ship->GetName());

        // If auto-docking is enabled and the pad is free, initiate docking
        if (bAutoDockOnEnter && CurrentDockingStatus == EDockingStatus::Available)
        {
            InitiateDocking(Ship);
        }
        // TODO: If not auto-docking, maybe notify the player controller/ship that docking is possible?
    }
}

void UDockingPadComponent::OnDockingVolumeOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
     // Server-only logic
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

     ASolaraqShipBase* Ship = Cast<ASolaraqShipBase>(OtherActor);
     // Check if it's a valid ship AND if the overlapping component is its physics root
    if (Ship && OtherComp == static_cast<UPrimitiveComponent*>(Ship->GetCollisionAndPhysicsRoot()))
    {
         UE_LOG(LogSolaraqSystem, Log, TEXT("Docking Pad %s: Ship %s left volume."), *GetNameSafe(this), *Ship->GetName());

        // Potential logic: If a ship leaves the volume *while* it was in the process of docking
        // (but not fully docked), cancel the dock? Or maybe prevent leaving? Depends on design.

        // Generally, leaving the volume doesn't automatically undock a fully docked ship.
        // Undocking usually requires explicit action.
    }
}

bool UDockingPadComponent::InitiateDocking(ASolaraqShipBase* ShipToDock)
{
    // --- SERVER ONLY ---
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(ShipToDock))
    {
        return false;
    }

    // Check if pad is available and the ship isn't already docked somewhere else
    if (CurrentDockingStatus == EDockingStatus::Available && !ShipToDock->IsDocked())
    {
         UE_LOG(LogSolaraqSystem, Log, TEXT("Docking Pad %s: Initiating docking for ship %s."), *GetNameSafe(this), *ShipToDock->GetName());

        // Set state immediately (will replicate)
        CurrentDockingStatus = EDockingStatus::Occupied; // Or EDockingStatus::Docking if using intermediate states
        CurrentDockedShip = ShipToDock;

        // Tell the SHIP to perform the actual docking procedure
        ShipToDock->Server_DockWithPad(this); // Pass self as the pad reference

        // Force replication updates immediately if needed (usually not necessary unless relying on OnRep instantly)
        // ForceNetUpdate();
        // ShipToDock->ForceNetUpdate();

        return true;
    }
    else
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("Docking Pad %s: Failed to initiate docking for ship %s. Status: %d, Ship Docked: %d"),
            *GetNameSafe(this), *ShipToDock->GetName(), CurrentDockingStatus, ShipToDock->IsDocked());
        return false;
    }
}

bool UDockingPadComponent::InitiateUndocking()
{
    // --- SERVER ONLY ---
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(CurrentDockedShip))
    {
        return false;
    }

    // Check if a ship is actually docked
    if (CurrentDockingStatus == EDockingStatus::Occupied && CurrentDockedShip->IsDockedTo(this))
    {
         UE_LOG(LogSolaraqSystem, Log, TEXT("Docking Pad %s: Initiating undocking for ship %s."), *GetNameSafe(this), *CurrentDockedShip->GetName());

        // Tell the SHIP to perform the undocking procedure
        CurrentDockedShip->Server_RequestUndock(); // Ship handles its own detachment and state

        // Clear local references AFTER telling ship (ship might need pad info during undock)
        CurrentDockingStatus = EDockingStatus::Available; // Or EDockingStatus::Undocking
        CurrentDockedShip = nullptr;

        // ForceNetUpdate(); // Optional

        return true;
    }
     else
    {
        UE_LOG(LogSolaraqSystem, Warning, TEXT("Docking Pad %s: Failed to initiate undocking. Status: %d, Current Ship: %s"),
            *GetNameSafe(this), CurrentDockingStatus, *GetNameSafe(CurrentDockedShip));
        return false;
    }
}

void UDockingPadComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UDockingPadComponent, CurrentDockingStatus);
    DOREPLIFETIME(UDockingPadComponent, CurrentDockedShip);
}

void UDockingPadComponent::OnRep_DockingStatus()
{
    // Called on CLIENTS when CurrentDockingStatus changes.
    // Update UI, visuals, etc.
     UE_LOG(LogSolaraqSystem, Verbose, TEXT("CLIENT Docking Pad %s: Status changed to %d"), *GetNameSafe(this), CurrentDockingStatus);
     // Example: Update visual material of the pad based on status
}

void UDockingPadComponent::OnRep_DockedShip()
{
    // Called on CLIENTS when CurrentDockedShip changes.
    // Update UI, maybe highlight the docked ship?
     UE_LOG(LogSolaraqSystem, Verbose, TEXT("CLIENT Docking Pad %s: Docked ship changed to %s"), *GetNameSafe(this), *GetNameSafe(CurrentDockedShip));
}