// Environment/DestructibleAsteroid.cpp
#include "Environment/DestructibleAsteroid.h"
#include "Kismet/GameplayStatics.h"
#include "Items/ItemPickup.h"         // Required to set the ItemData
#include "Items/ItemDataAssetBase.h" 

ADestructibleAsteroid::ADestructibleAsteroid() :
    LootDropChance(1.0f) // Keep at 100% for testing
{
    MaxHealth = 150.0f;
    MinSignificantDamageToFracture = 30.0f;
}

void ADestructibleAsteroid::BeginPlay()
{
    Super::BeginPlay(); // CRITICAL: Call parent's BeginPlay!

    // Any asteroid-specific initialization
    // UE_LOG(LogSolaraq, Log, TEXT("DestructibleAsteroid %s initialized."), *GetName());
    UE_LOG(LogTemp, Log, TEXT("DestructibleAsteroid %s initialized."), *GetName());
}

void ADestructibleAsteroid::OnFullyDestroyed_Implementation(AActor* DamageCauser)
{
    Super::OnFullyDestroyed_Implementation(DamageCauser);

    // 1. Validation
    if (!LootPickupClass || PossibleLootItems.Num() == 0) return;

    // 2. Chance Roll
    if (FMath::FRand() > LootDropChance) return;

    // 3. Select Random Data Asset
    const int32 Index = FMath::RandRange(0, PossibleLootItems.Num() - 1);
    UItemDataAssetBase* SelectedItemData = PossibleLootItems[Index];

    if (!SelectedItemData) return;

    // 4. Deferred Spawn (Spawn -> Configure -> Finish)
    FVector SpawnLoc = GetActorLocation();
    FRotator SpawnRot = FMath::VRand().Rotation();
    
    AItemPickup* NewPickup = GetWorld()->SpawnActorDeferred<AItemPickup>(
        LootPickupClass, 
        FTransform(SpawnRot, SpawnLoc), 
        this, 
        GetInstigator(), 
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
    );

    if (NewPickup)
    {
        NewPickup->ItemData = SelectedItemData;
        NewPickup->Quantity = FMath::RandRange(1, 3); // Or whatever logic you prefer

        UGameplayStatics::FinishSpawningActor(NewPickup, FTransform(SpawnRot, SpawnLoc));

        // Add small physics drift
        UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(NewPickup->GetRootComponent());
        if (RootPrim && RootPrim->IsSimulatingPhysics())
        {
            RootPrim->AddImpulse(FMath::VRand() * 50.0f, NAME_None, true);
        }
    }
}