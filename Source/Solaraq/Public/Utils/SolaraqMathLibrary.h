#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SolaraqMathLibrary.generated.h"

UCLASS()
class SOLARAQ_API USolaraqMathLibrary : public UBlueprintFunctionLibrary // Replace YOURPROJECT_API
{
    GENERATED_BODY()

public:
    /**
     * Calculates the future intercept point of a target moving at a constant velocity,
     * given a shooter's position, velocity, and projectile speed.
     * @param ShooterLocation Current location of the shooter/projectile.
     * @param ShooterVelocity Current velocity of the shooter/projectile (can be zero if projectile is just starting).
     * @param TargetLocation Current location of the target.
     * @param TargetVelocity Current velocity of the target.
     * @param ProjectileSpeed The speed of the projectile.
     * @param OutInterceptPoint (Output) The calculated intercept point.
     * @return True if a valid intercept point is found, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = "Solaraq|Math")
    static bool CalculateInterceptPoint(
        FVector ShooterLocation,
        FVector ShooterVelocity,
        FVector TargetLocation,
        FVector TargetVelocity,
        float ProjectileSpeed,
        FVector& OutInterceptPoint
    );
};