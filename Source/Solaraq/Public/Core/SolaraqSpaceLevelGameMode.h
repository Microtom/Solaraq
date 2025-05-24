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

	virtual void RestartPlayer(AController* NewPlayer) override;
	void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage);
	bool PlayerCanRestart(APlayerController* Player);
	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;


protected:
	// Assign this in Blueprint if you have a specific player ship BP to spawn when returning
	UPROPERTY(EditDefaultsOnly, Category = "Solaraq|Player")
	TSubclassOf<ASolaraqShipBase> PlayerShipClassToSpawn; 
};