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
UCLASS(Abstract)
class SOLARAQ_API AItemActorBase : public AActor
{
    GENERATED_BODY()
    
public:    
    AItemActorBase();

    // --- Public Functions ---

    // This function is called in the editor whenever a property is changed.
    virtual void OnConstruction(const FTransform& Transform) override;
    
    // Sets the pawn that is holding this item. Called by EquipmentComponent.
    virtual void SetOwningPawn(ASolaraqCharacterPawn* NewOwner);
    
    // Called when the item is equipped, after the owner has been set.
    virtual void OnEquip();
    
    // Called when the item is unequipped, before being destroyed.
    virtual void OnUnequip();

    // ... (PrimaryUse, SecondaryUse functions are fine) ...
    virtual void PrimaryUse();
    virtual void PrimaryUse_Stop();
    virtual void SecondaryUse();
    virtual void SecondaryUse_Stop();


protected:

    virtual void OnItemDataChanged();
    
    // The pawn that is currently holding this item actor
    UPROPERTY(BlueprintReadOnly, Category = "Solaraq|Item")
    TObjectPtr<ASolaraqCharacterPawn> OwningPawn;

    // The data asset that defines this item
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Item")
    TObjectPtr<UItemDataAssetBase> ItemData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    TObjectPtr<USceneComponent> DefaultSceneRoot;
};