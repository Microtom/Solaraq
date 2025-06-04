// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "MiningDamageType.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, Const) // Const because typically DamageType CDOs are used
class SOLARAQ_API UMiningDamageType : public UDamageType
{
	GENERATED_BODY()

public:
	UMiningDamageType();
	
};
