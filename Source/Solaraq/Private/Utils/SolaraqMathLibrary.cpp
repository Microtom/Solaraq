#include "Utils/SolaraqMathLibrary.h" // Adjust include path as necessary
#include "Math/UnrealMathUtility.h"
// You might need: #include "Logging/LogMacros.h" if you add logs here
// DEFINE_LOG_CATEGORY_STATIC(LogSolaraqMath, Log, All); // Example for logging

bool USolaraqMathLibrary::CalculateInterceptPoint(
    FVector ShooterLocation, FVector ShooterVelocity,
    FVector TargetLocation, FVector TargetVelocity,
    float ProjectileSpeed, FVector& OutInterceptPoint, float& OutTimeToIntercept)
{
    // Initialize out parameters
    OutInterceptPoint = TargetLocation; // Default to target's current location on failure
    OutTimeToIntercept = -1.0f;         // Default to invalid time

    if (ProjectileSpeed <= 0.0f)
    {
        // UE_LOG(LogSolaraqMath, Warning, TEXT("CalculateInterceptPoint: ProjectileSpeed is non-positive (%.2f). Cannot intercept."), ProjectileSpeed);
        return false; // Cannot intercept if projectile has no speed or negative speed
    }

    const FVector RelativePosition = TargetLocation - ShooterLocation;
    const FVector RelativeVelocity = TargetVelocity - ShooterVelocity;

    // Quadratic equation components: at^2 + bt + c = 0
    // a = Vr.Vr - S^2
    // b = 2 * P.Vr
    // c = P.P
    // Where Vr = RelativeVelocity, P = RelativePosition, S = ProjectileSpeed
    const float a = FVector::DotProduct(RelativeVelocity, RelativeVelocity) - FMath::Square(ProjectileSpeed);
    const float b = 2.0f * FVector::DotProduct(RelativePosition, RelativeVelocity);
    const float c = FVector::DotProduct(RelativePosition, RelativePosition);

    float t = -1.0f; // Time to intercept, initialized to invalid

    // Check if 'a' is nearly zero (linear equation or no solution)
    if (FMath::IsNearlyZero(a))
    {
        // Linear equation: bt + c = 0, so t = -c / b
        if (!FMath::IsNearlyZero(b))
        {
            t = -c / b;
        }
        // else: a and b are both zero.
        // If c is also zero, they are at the same point and moving at speeds that maintain this (or S = |Vr|). Any t > 0 could be an intercept.
        // However, for a predictive sense, this case is often treated as no definitive future intercept unless P is also zero.
        // If c is non-zero, they are parallel and will never meet (or are moving apart with S = |Vr|).
        // For simplicity, if a and b are zero, we consider it a failure to find a unique positive t.
    }
    else // Standard quadratic equation
    {
        const float Discriminant = FMath::Square(b) - 4.0f * a * c;

        if (Discriminant >= 0.0f) // Real solutions exist
        {
            const float SqrtDiscriminant = FMath::Sqrt(Discriminant);
            
            // Standard quadratic formula: t = (-b +/- sqrt(Discriminant)) / (2a)
            const float t1 = (-b + SqrtDiscriminant) / (2.0f * a);
            const float t2 = (-b - SqrtDiscriminant) / (2.0f * a);

            // We need the smallest positive time to intercept.
            // Initialize best_t to a very large number or an invalid marker.
            float best_t = -1.0f;

            if (t1 > KINDA_SMALL_NUMBER) // t1 is a potential future intercept
            {
                best_t = t1;
            }

            if (t2 > KINDA_SMALL_NUMBER) // t2 is a potential future intercept
            {
                if (best_t < 0.0f || t2 < best_t) // If t1 was invalid, or t2 is smaller than t1
                {
                    best_t = t2;
                }
            }
            t = best_t; // Assign the chosen smallest positive time
        }
        // Else: Discriminant < 0, no real solutions (e.g., target is too fast, moving away, or projectile too slow).
        // t remains -1.0f
    }

    // Check if a valid positive time was found
    if (t > KINDA_SMALL_NUMBER) // Using KINDA_SMALL_NUMBER to avoid issues with t being extremely close to zero
    {
        OutInterceptPoint = TargetLocation + (TargetVelocity * t);
        OutTimeToIntercept = t;
        // UE_LOG(LogSolaraqMath, Verbose, TEXT("CalculateInterceptPoint: Success. Time: %.3f, Intercept: %s"), t, *OutInterceptPoint.ToString());
        return true;
    }
    else
    {
        // Fallback: OutInterceptPoint is already TargetLocation, OutTimeToIntercept is already -1.0f
        // UE_LOG(LogSolaraqMath, Warning, TEXT("CalculateInterceptPoint: Failed. No valid positive time. t = %.3f. a=%.2f, b=%.2f, c=%.2f"), t, a,b,c);
        return false;
    }
}