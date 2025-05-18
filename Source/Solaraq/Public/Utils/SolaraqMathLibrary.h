#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SolaraqMathLibrary.generated.h"

UCLASS()
class SOLARAQ_API USolaraqMathLibrary : public UBlueprintFunctionLibrary // Or UObject if not BP callable
{
    GENERATED_BODY()

public:
    /**
     * Calculates the future intercept point of a projectile with a target.
     * Assumes constant velocities for both shooter and target from the moment of calculation.
     * @param ShooterLocation Current location of the shooter.
     * @param ShooterVelocity Current velocity of the shooter.
     * @param TargetLocation Current location of the target.
     * @param TargetVelocity Current velocity of the target.
     * @param ProjectileSpeed Speed of the projectile (scalar).
     * @param OutInterceptPoint [OUT] The calculated world location of the intercept. If prediction fails, this will be TargetLocation.
     * @param OutTimeToIntercept [OUT] The time in seconds until intercept. If prediction fails or time is non-positive, this will be < 0.
     * @return True if a valid future intercept point was found, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = "Solaraq|Math") // Make static if you prefer calling directly without an object instance
    static bool CalculateInterceptPoint(
        FVector ShooterLocation, FVector ShooterVelocity,
        FVector TargetLocation, FVector TargetVelocity,
        float ProjectileSpeed, FVector& OutInterceptPoint, float& OutTimeToIntercept);
};