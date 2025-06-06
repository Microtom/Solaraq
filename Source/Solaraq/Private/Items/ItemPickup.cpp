// ItemPickup.cpp
#include "Items/ItemPickup.h"
#include "Items/ItemDataAssetBase.h"
#include "Items/InventoryComponent.h"
#include "Pawns/SolaraqCharacterPawn.h" // We need to know about the pawn
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

AItemPickup::AItemPickup()
{
    PrimaryActorTick.bCanEverTick = false;

    OverlapSphere = CreateDefaultSubobject<USphereComponent>(TEXT("OverlapSphere"));
    RootComponent = OverlapSphere;
    OverlapSphere->SetSphereRadius(100.0f);
    OverlapSphere->SetCollisionProfileName(TEXT("Trigger")); // So we can walk through it

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(RootComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // The mesh itself doesn't need to collide
}

void AItemPickup::BeginPlay()
{
    Super::BeginPlay();
    
    // Set up the overlap event
    OverlapSphere->OnComponentBeginOverlap.AddDynamic(this, &AItemPickup::OnSphereOverlap);

    // Set the visual mesh based on our ItemData
    if (ItemData && ItemData->PickupMesh)
    {
        MeshComponent->SetStaticMesh(ItemData->PickupMesh);
    }
}

void AItemPickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Try to cast the overlapping actor to our character pawn
    ASolaraqCharacterPawn* CharacterPawn = Cast<ASolaraqCharacterPawn>(OtherActor);
    if (CharacterPawn)
    {
        // Get the character's inventory component
        UInventoryComponent* Inventory = CharacterPawn->GetInventoryComponent();
        if (Inventory && ItemData)
        {
            // Try to add the item to the inventory
            const int32 UnaddedQuantity = Inventory->AddItem(ItemData, Quantity);

            if (UnaddedQuantity == 0)
            {
                // If everything was added successfully, destroy this pickup actor
                Destroy();
            }
            else
            {
                // If the inventory was full, update our quantity to what's left
                Quantity = UnaddedQuantity;
            }
        }
    }
}