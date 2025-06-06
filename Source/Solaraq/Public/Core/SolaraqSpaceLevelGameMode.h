// SolaraqSpaceLevelGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SolaraqSpaceLevelGameMode.generated.h"

class ASolaraqShipBase;

UCLASS()
class SOLARAQ_API ASolaraqSpaceLevelGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASolaraqSpaceLevelGameMode();

	// Override the Tick function to add our logging
	virtual void Tick(float DeltaSeconds) override;
	
	// This UPROPERTY is specific to your game for easily selecting the ship pawn.
	// It's fine to keep it if you prefer this direct slot.
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Player", meta = (DisplayName = "Player Ship Class To Spawn"))
	TSubclassOf<ASolaraqShipBase> PlayerShipClassToSpawn;

protected:
	// We will use this to ensure PlayerShipClassToSpawn is prioritized if set.
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

public:
	virtual void RestartPlayer(AController* NewPlayer) override;
	// ... (other overrides like FindPlayerStart, InitGame, PlayerCanRestart remain)
	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual bool PlayerCanRestart(APlayerController* Player);

private:
	float TimeSinceLastLog = 0.0f;
	const float LogInterval = 2.0f;

};