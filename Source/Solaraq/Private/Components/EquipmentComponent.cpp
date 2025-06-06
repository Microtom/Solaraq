// EquipmentComponent.cpp
#include "Components/EquipmentComponent.h"
#include "Items/ItemActorBase.h"          // FIXED: Include the base class for our equippable items
#include "Items/ItemDataAssetBase.h"     // FIXED: Include the data asset class
#include "Pawns/SolaraqCharacterPawn.h"   // FIXED: Include the pawn for casting
#include "GameFramework/Character.h"      // For attaching to the mesh
#include "Items/ItemToolDataAsset.h"

UEquipmentComponent::UEquipmentComponent()
{
    // We don't need this component to tick by itself.
    PrimaryComponentTick.bCanEverTick = false; 
}

void UEquipmentComponent::BeginPlay()
{
    Super::BeginPlay();
}

ASolaraqCharacterPawn* UEquipmentComponent::GetOwnerPawn() const
{
    return Cast<ASolaraqCharacterPawn>(GetOwner());
}

void UEquipmentComponent::EquipItem(UItemDataAssetBase* ItemToEquip)
{
    UItemToolDataAsset* ToolData = Cast<UItemToolDataAsset>(ItemToEquip);

    // Only proceed if the cast was successful (meaning, it IS a tool) and we have a valid owner.
    if (!ToolData || !GetOwnerPawn())
    {
        // Optional: Add a log here if you want to know when an invalid item is attempted to be equipped.
        // UE_LOG(LogTemp, Warning, TEXT("EquipItem failed: Item is not a valid tool or owner is null."));
        return;
    }

    // First, unequip any existing item
    UnequipItem();

    // Now, spawn and attach the new item actor using the properties from the ToolData
    if (UClass* ActorClass = ToolData->EquippableActorClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = GetOwnerPawn();
        SpawnParams.Instigator = GetOwnerPawn();

        AItemActorBase* NewItemActor = GetWorld()->SpawnActor<AItemActorBase>(ActorClass, GetOwner()->GetActorLocation(), GetOwner()->GetActorRotation(), SpawnParams);
        if (NewItemActor)
        {
            CurrentEquippedActor = NewItemActor;
            CurrentEquippedItemData = ToolData; // We can store the casted pointer
            
            NewItemActor->SetOwningPawn(GetOwnerPawn());

            if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
            {
                // FIXED: Use the AttachmentSocket from the successfully casted ToolData
                NewItemActor->AttachToComponent(OwnerChar->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, ToolData->AttachmentSocket);
            }

            NewItemActor->OnEquip();
        }
    }
}

void UEquipmentComponent::UnequipItem()
{
    if (CurrentEquippedActor)
    {
        // Call the item's unequip logic BEFORE destroying it
        CurrentEquippedActor->OnUnequip();
        CurrentEquippedActor->Destroy();
    }
    CurrentEquippedActor = nullptr;
    CurrentEquippedItemData = nullptr;
}


void UEquipmentComponent::HandlePrimaryUse()
{
    if (CurrentEquippedActor)
    {
        CurrentEquippedActor->PrimaryUse();
    }
}

void UEquipmentComponent::HandlePrimaryUse_Stop()
{
    if (CurrentEquippedActor)
    {
        CurrentEquippedActor->PrimaryUse_Stop();
    }
}

void UEquipmentComponent::HandleSecondaryUse()
{
    if (CurrentEquippedActor)
    {
        CurrentEquippedActor->SecondaryUse();
    }
}

void UEquipmentComponent::HandleSecondaryUse_Stop()
{
    if (CurrentEquippedActor)
    {
        CurrentEquippedActor->SecondaryUse_Stop();
    }
}