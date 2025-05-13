#include "Utils/SolaraqMathLibrary.h" // Adjust include path as necessary
#include "Math/UnrealMathUtility.h"

bool USolaraqMathLibrary::CalculateInterceptPoint(
    FVector ShooterLocation, FVector ShooterVelocity,
    FVector TargetLocation, FVector TargetVelocity,
    float ProjectileSpeed, FVector& OutInterceptPoint)
{
    const FVector RelativePosition = TargetLocation - ShooterLocation;
    const FVector RelativeVelocity = TargetVelocity - ShooterVelocity;

    // Quadratic equation components: at^2 + bt + c = 0
    const float a = FVector::DotProduct(RelativeVelocity, RelativeVelocity) - FMath::Square(ProjectileSpeed);
    const float b = 2.0f * FVector::DotProduct(RelativePosition, RelativeVelocity);
    const float c = FVector::DotProduct(RelativePosition, RelativePosition);

    float t = -1.0f; // Time to intercept, initialized to invalid

    if (FMath::IsNearlyZero(a)) // Linear equation: bt + c = 0 (relative speed approx. projectile speed)
    {
        if (!FMath::IsNearlyZero(b))
        {
            t = -c / b;
        }
        // else: a and b are zero. If c is also zero, they are already at the same point.
        // If c is non-zero, they are parallel and will never intercept (or are moving apart).
    }
    else
    {
        const float Discriminant = FMath::Square(b) - 4.0f * a * c;

        if (Discriminant >= 0.0f) // Real solutions exist
        {
            const float SqrtDiscriminant = FMath::Sqrt(Discriminant);
            const float t1 = (-b + SqrtDiscriminant) / (2.0f * a);
            const float t2 = (-b - SqrtDiscriminant) / (2.0f * a);

            // Select the smallest positive time
            if (t1 > KINDA_SMALL_NUMBER && (t2 <= KINDA_SMALL_NUMBER || t1 < t2))
            {
                t = t1;
            }
            else if (t2 > KINDA_SMALL_NUMBER)
            {
                t = t2;
            }
            // Else: Both solutions are non-positive or NaN, no future intercept.
        }
        // Else: Discriminant < 0, no real solutions (e.g., target is too fast or moving away too quickly).
    }

    if (t > KINDA_SMALL_NUMBER) // A valid positive time to intercept was found
    {
        OutInterceptPoint = TargetLocation + TargetVelocity * t;
        return true;
    }
    else
    {
        // Fallback: Aim directly at the target's current position if prediction fails
        OutInterceptPoint = TargetLocation;
        return false;
    }
}