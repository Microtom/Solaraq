// ItemActor_FishingRod.h
#pragma once

#include "CoreMinimal.h"
#include "Items/ItemActorBase.h"
#include "ItemActor_FishingRod.generated.h"

// Forward Declarations
class USplineComponent;
class USplineMeshComponent;
class AFishingBobber;
class USoundBase;
class UMaterialInterface;

UCLASS()
class SOLARAQ_API AItemActor_FishingRod : public AItemActorBase
{
    GENERATED_BODY()

public:
    AItemActor_FishingRod(); // FIXED: Added constructor declaration

    //~ Begin AItemActorBase Interface
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnUnequip() override;
    virtual void PrimaryUse() override;
    virtual void PrimaryUse_Stop() override;
    //~ End AItemActorBase Interface

    // --- Public API for FishingSubsystem ---
    AFishingBobber* SpawnAndCastBobber(float Charge);
    void StartReeling();
    void NotifyFishBite();
    void NotifyReset();

protected:
    // FIXED: Added all missing component and property declarations
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    TObjectPtr<USkeletalMeshComponent> RodMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    TObjectPtr<USplineComponent> FishingLineSpline;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Solaraq|Components")
    TObjectPtr<USplineMeshComponent> FishingLineMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Fishing Rod")
    FName RodTipSocketName = "RodTipSocket";

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Fishing Rod")
    TSubclassOf<AFishingBobber> BobberClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Fishing Rod")
    TObjectPtr<UStaticMesh> FishingLineStaticMesh;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Fishing Rod")
    TObjectPtr<UMaterialInterface> FishingLineMaterial;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solaraq|Fishing Rod|Audio")
    TObjectPtr<USoundBase> FishBiteSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Fishing Rod")
    float CastPower = 1500.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Solaraq|Fishing Rod")
    float ReelSpeed = 1000.f;

private:
    void UpdateLine(); // FIXED: Added missing function declaration
    
    UPROPERTY()
    TObjectPtr<AFishingBobber> CurrentBobber;
    
    bool bIsReeling = false;
};