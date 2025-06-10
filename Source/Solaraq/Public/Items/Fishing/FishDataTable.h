// FishDataTable.h (or any appropriate file)
#pragma once

#include "Engine/DataTable.h"
#include "FishDataTable.generated.h"

class UItemDataAssetBase;

USTRUCT(BlueprintType)
struct FFishLootEntry : public FTableRowBase
{
	GENERATED_BODY()

	// The fish item that can be caught
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UItemDataAssetBase> FishItemData;

	// How common is this fish? Higher numbers are more common.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Weight = 100;
};