// ItemActorBase.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemActorBase.generated.h"

class UItemDataAssetBase;
class ASolaraqCharacterPawn;
class UItemDataAsset;

/**
 * The base class for all actors that are spawned and equipped by the player.
 * This includes tools, weapons, fishing rods, etc.
 */
UCLASS(Abstract) // Abstract as we won't spawn this directly
class SOLARAQ_API AItemActorBase : public AActor
{
    GENERATED_BODY()
    
public:    
    AItemActorBase();

    // The pawn that is currently holding this item actor
    UPROPERTY(BlueprintReadOnly, Category = "Solaraq|Item")
    TObjectPtr<ASolaraqCharacterPawn> OwningPawn;

    // The data asset that defines this item
    UPROPERTY(BlueprintReadOnly, Category = "Solaraq|Item")
    TObjectPtr<UItemDataAssetBase> ItemData;
    
    // Called when the item is equipped
    virtual void OnEquip(ASolaraqCharacterPawn* NewOwner);
    
    // Called when the item is unequipped
    virtual void OnUnequip();

    // Called when the player presses the "Primary Use" button (e.g., Fire)
    UFUNCTION(BlueprintCallable, Category = "Solaraq|Item")
    virtual void PrimaryUse();

    // Called when the player releases the "Primary Use" button
    UFUNCTION(BlueprintCallable, Category = "Solaraq|Item")
    virtual void PrimaryUse_Stop();

    // Called when the player presses the "Secondary Use" button (e.g., Aim)
    UFUNCTION(BlueprintCallable, Category = "Solaraq|Item")
    virtual void SecondaryUse();

    // Called when the player releases the "Secondary Use" button
    UFUNCTION(BlueprintCallable, Category = "Solaraq|Item")
    virtual void SecondaryUse_Stop();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    TObjectPtr<USceneComponent> DefaultSceneRoot;
};