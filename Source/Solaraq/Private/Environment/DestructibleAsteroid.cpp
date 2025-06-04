#include "Environment/DestructibleAsteroid.h"
#include "Kismet/GameplayStatics.h"
// #include "Logging/SolaraqLogChannels.h"

ADestructibleAsteroid::ADestructibleAsteroid() :
    LootDropChance(0.3f) // Default loot drop chance
{
    // Set any default values specific to asteroids that might differ from DestructibleBase defaults
    // For example, asteroids might generally have more health than small debris
    MaxHealth = 150.0f;
    MinSignificantDamageToFracture = 30.0f;

    // The GeometryCollectionComponent is already created and set up by the base class.
    // You would assign the specific Geometry Collection asset (GC_Asteroid) in the Blueprint
    // derived from this C++ class, or you could hardcode a path here if all asteroids use the same GC.
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
    Super::OnFullyDestroyed_Implementation(DamageCauser); // IMPORTANT: Call base implementation to play common effects, etc.

    // UE_LOG(LogSolaraq, Log, TEXT("DestructibleAsteroid %s: OnFullyDestroyed_Implementation. Spawning loot if lucky."), *GetName());
    UE_LOG(LogTemp, Log, TEXT("DestructibleAsteroid %s: OnFullyDestroyed_Implementation. Spawning loot if lucky."), *GetName());


    // Asteroid-specific logic: Spawn loot
    if (PossibleLootDrops.Num() > 0 && FMath::FRand() < LootDropChance)
    {
        const int32 LootIndex = FMath::RandRange(0, PossibleLootDrops.Num() - 1);
        if (TSubclassOf<AActor> LootClass = PossibleLootDrops[LootIndex])
        {
            FVector SpawnLocation = GetActorLocation(); // Or a location from the BreakEvent if preferred
            FRotator SpawnRotation = FRotator::ZeroRotator; // Or random rotation

            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.Instigator = GetInstigator();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

            GetWorld()->SpawnActor<AActor>(LootClass, SpawnLocation, SpawnRotation, SpawnParams);
            // UE_LOG(LogSolaraq, Log, TEXT("Asteroid %s dropped loot: %s"), *GetName(), *LootClass->GetName());
            UE_LOG(LogTemp, Log, TEXT("Asteroid %s dropped loot: %s"), *GetName(), *LootClass->GetName());
        }
    }
}

// Example of overriding piece broken if needed:
/*
void ADestructibleAsteroid::OnPieceBroken_Implementation(const FVector& PieceLocation, const FVector& PieceImpulseDir)
{
    Super::OnPieceBroken_Implementation(PieceLocation, PieceImpulseDir); // Call base for common piece break effects

    // Asteroid-specific effect when a piece breaks off, e.g., spawn extra dust.
    // UE_LOG(LogSolaraq, Verbose, TEXT("Asteroid %s: A piece broke off. Playing custom asteroid dust effect."), *GetName());
}
*/// Fill out your copyright notice in the Description page of Project Settings.


#include "Environment/DestructibleAsteroid.h"

