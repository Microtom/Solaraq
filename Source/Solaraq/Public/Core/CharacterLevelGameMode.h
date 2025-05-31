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

	// No custom UPROPERTY needed for DefaultPawnClass or PlayerControllerClass here,
	// as we'll just set the inherited ones in C++ and they can be overridden in BP.

public:
	// ... (overrides for InitGame, RestartPlayer, FindPlayerStart remain) ...
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;
};