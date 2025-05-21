// CharacterLevelGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CharacterLevelGameMode.generated.h"

UCLASS()
class SOLARAQ_API ACharacterLevelGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACharacterLevelGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override; 
	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;
};