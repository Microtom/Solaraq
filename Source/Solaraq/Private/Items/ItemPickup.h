// ItemPickup.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemPickup.generated.h"

class UItemDataAssetBase;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class SOLARAQ_API AItemPickup : public AActor
{
	GENERATED_BODY()
    
public:    
	AItemPickup();

	// The item data this pickup represents
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TObjectPtr<UItemDataAssetBase> ItemData;

	// The quantity of the item in this pickup
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	int32 Quantity = 1;

protected:
	virtual void BeginPlay() override;
    
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> OverlapSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};