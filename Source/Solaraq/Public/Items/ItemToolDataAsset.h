// ItemToolDataAsset.h
#pragma once

#include "CoreMinimal.h"
#include "ItemDataAssetBase.h"
#include "ItemToolDataAsset.generated.h"

// Forward Declaration
class AItemActorBase;
class UAnimInstance;

/**
 * Data Asset for items that can be equipped by the player and used as a tool (e.g., fishing rod, mining laser, scanner).
 */
UCLASS(BlueprintType) // Make it Blueprintable so we can create instances
class SOLARAQ_API UItemToolDataAsset : public UItemDataAssetBase
{
	GENERATED_BODY()

public:
	UItemToolDataAsset()
	{
		// Tools are typically unique items, not stackable.
		ItemType = EItemType::Tool;
		bIsStackable = false;
		MaxStackSize = 1;
	}

	// The AItemActorBase child class to spawn when this item is equipped.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool")
	TSubclassOf<AItemActorBase> EquippableActorClass;

	// The Skeletal Mesh to display for this tool.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool")
	TObjectPtr<USkeletalMesh> ToolSkeletalMesh;

	// The Animation Blueprint class to use with the mesh.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool")
	TSubclassOf<UAnimInstance> ToolAnimClass;
	
	// The name of the socket on the character's mesh to attach this tool to.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool")
	FName AttachmentSocket = "hand_r_socket"; // Provide a sensible default
};