// SolaraqGimbalGunComponent.cpp

#include "Components/SolaraqGimbalGunComponent.h"
#include "Pawns/SolaraqShipBase.h" // For casting owner and getting team
#include "Projectiles/SolaraqProjectile.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h" // For getting mouse
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Logging/SolaraqLogChannels.h" // Your custom log channel

#if WITH_EDITOR
#include "DrawDebugHelpers.h"

#endif

USolaraqGimbalGunComponent::USolaraqGimbalGunComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics; // Tick after physics to get latest positions

    // --- Create Gun Mesh Sub-Component ---
    GunMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
    GunMeshComponent->SetupAttachment(this); // Attach to this SceneComponent
    GunMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
    GunMeshComponent->SetIsReplicated(false); // Mesh visuals are driven by replicated yaw

    // --- Default Values ---
    ProjectileClass = nullptr;
    MuzzleSocketName = NAME_None;
    MuzzleOffset = FVector(50.0f, 0.0f, 0.0f); // Default forward offset
    FireRate = 2.0f; // shots per second
    ProjectileMuzzleSpeed = 5000.0f;
    BaseDamage = 10.0f;
    LastFireTime = -FireRate; // Allow firing immediately

    MaxYawRotationSpeed = 180.0f; // degrees per second
    CurrentActualGimbalRelativeYaw = 0.0f;
    DesiredGimbalRelativeYaw = 0.0f;
    ClientVisualGimbalRelativeYaw = 0.0f;

    bEnableYawConstraints = false;
    ConstraintCenterRelativeYaw = 0.0f;
    MaxYawAngleFromCenter = 90.0f; // Default to a 180 degree total arc if centered at 0

    SetIsReplicatedByDefault(true); // This component itself replicates
}

void USolaraqGimbalGunComponent::BeginPlay()
{
    Super::BeginPlay();

    // Ensure LastFireTime allows immediate firing if desired
    if (GetOwner() && GetOwner()->GetLocalRole() == ROLE_Authority)
    {
        LastFireTime = -1.0f / FireRate; // Time for one shot in the past
    }

    // Initialize visual yaw to actual yaw
    ClientVisualGimbalRelativeYaw = CurrentActualGimbalRelativeYaw;
    if (GunMeshComponent)
    {
        GunMeshComponent->SetRelativeRotation(FRotator(0.f, ClientVisualGimbalRelativeYaw, 0.f));
    }

    // Try to get OwningPawn and TeamID if not set explicitly
    if (!OwningPawn.IsValid())
    {
        SetOwningPawn(Cast<APawn>(GetOwner()));
    }
}

void USolaraqGimbalGunComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(USolaraqGimbalGunComponent, CurrentActualGimbalRelativeYaw, COND_SkipOwner); // Replicate to non-owners for visuals
}

void USolaraqGimbalGunComponent::SetOwningPawn(APawn* NewOwningPawn)
{
    OwningPawn = NewOwningPawn;
    if (OwningPawn.IsValid())
    {
        // Try to get TeamId from the pawn (or its controller)
        IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(OwningPawn.Get());
        if (TeamAgent)
        {
            TeamId = TeamAgent->GetGenericTeamId();
        }
        else if (OwningPawn->GetController())
        {
            TeamAgent = Cast<IGenericTeamAgentInterface>(OwningPawn->GetController());
            if (TeamAgent)
            {
                TeamId = TeamAgent->GetGenericTeamId();
            }
        }
    }
    else
    {
        TeamId = FGenericTeamId::NoTeam;
    }
}

FGenericTeamId USolaraqGimbalGunComponent::GetGenericTeamId() const
{
    // If we have an owning pawn with a team, use that.
    // Otherwise, this component might be on a neutral turret.
    return TeamId;
}

void USolaraqGimbalGunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AActor* MyOwner = GetOwner();
    if (!MyOwner || !OwningPawn.IsValid()) return;

    // --- Server Authoritative Rotation ---
    if (MyOwner->HasAuthority())
    {
        // Server smoothly interpolates its CurrentActualGimbalRelativeYaw towards DesiredGimbalRelativeYaw
        // DesiredGimbalRelativeYaw is set by Server_SetDesiredYaw (RPC from client) or directly by AI
        float ClampedDesiredYaw = GetClampedRelativeYaw(DesiredGimbalRelativeYaw);

        // Check if we are already at the target yaw (within a small tolerance)
        // FMath::FindDeltaAngleDegrees handles wrap-around from -180 to 180
        float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentActualGimbalRelativeYaw, ClampedDesiredYaw);

        if (FMath::Abs(DeltaYaw) > KINDA_SMALL_NUMBER) // If not already at target
        {
            float MaxRotationThisFrame = MaxYawRotationSpeed * DeltaTime;
            float RotationAmount = FMath::Clamp(DeltaYaw, -MaxRotationThisFrame, MaxRotationThisFrame);
            CurrentActualGimbalRelativeYaw = FRotator::NormalizeAxis(CurrentActualGimbalRelativeYaw + RotationAmount);
        }
        // CurrentActualGimbalRelativeYaw will be replicated to other clients.
        // The owning client uses ClientVisualGimbalRelativeYaw for its own mesh.
    }

    // --- Client Visual Rotation (Owner and Others) ---
    // All clients (owner and remote) update their GunMeshComponent's visual rotation.
    // The owning client interpolates towards its *locally set* DesiredGimbalRelativeYaw for responsiveness.
    // Remote clients interpolate towards the *replicated* CurrentActualGimbalRelativeYaw.

    float TargetVisualYaw = CurrentActualGimbalRelativeYaw; // Default for remote clients

    APlayerController* PC = Cast<APlayerController>(OwningPawn->GetController());
    if (PC && PC->IsLocalController()) // If this is the locally controlled player's gun
    {
        // Owning client aims towards its own desired yaw (which it sent to server) for responsiveness
        TargetVisualYaw = GetClampedRelativeYaw(DesiredGimbalRelativeYaw);
    }
    
    // Interpolate ClientVisualGimbalRelativeYaw
    float DeltaVisualYaw = FMath::FindDeltaAngleDegrees(ClientVisualGimbalRelativeYaw, TargetVisualYaw);
    if (FMath::Abs(DeltaVisualYaw) > KINDA_SMALL_NUMBER)
    {
        float MaxRotationThisFrame = MaxYawRotationSpeed * DeltaTime * 2.0f; // Allow client visuals to be a bit faster for snappiness
        float RotationAmount = FMath::Clamp(DeltaVisualYaw, -MaxRotationThisFrame, MaxRotationThisFrame);
        ClientVisualGimbalRelativeYaw = FRotator::NormalizeAxis(ClientVisualGimbalRelativeYaw + RotationAmount);
    }
    else
    {
        ClientVisualGimbalRelativeYaw = TargetVisualYaw; // Snap if very close
    }


    if (GunMeshComponent)
    {
        GunMeshComponent->SetRelativeRotation(FRotator(0.f, ClientVisualGimbalRelativeYaw, 0.f));
    }

#if WITH_EDITOR
    // Draw constraint arc in editor when selected
    if (bEnableYawConstraints && IsSelectedInEditor())
    {
        DrawConstraintArc();
    }
#endif
}

void USolaraqGimbalGunComponent::AimAtWorldLocation(const FVector& WorldTargetLocation)
{
    AActor* MyOwner = GetOwner();
    if (!MyOwner) return;

    FVector DirectionToTargetWorld = (WorldTargetLocation - GetComponentLocation()).GetSafeNormal();
    if (DirectionToTargetWorld.IsNearlyZero()) return;

    // Transform world direction to local space of the parent component (or actor if no parent component)
    FVector ParentForward = GetAttachParent() ? GetAttachParent()->GetForwardVector() : MyOwner->GetActorForwardVector();
    FQuat ParentRotation = GetAttachParent() ? GetAttachParent()->GetComponentQuat() : MyOwner->GetActorQuat();
    
    FVector DirectionToTargetLocalToParent = ParentRotation.UnrotateVector(DirectionToTargetWorld);
    DirectionToTargetLocalToParent.Z = 0; // Flatten to XY plane relative to parent
    DirectionToTargetLocalToParent.Normalize();

    // Calculate yaw angle relative to parent's forward
    float NewDesiredYaw = FMath::RadiansToDegrees(FMath::Atan2(DirectionToTargetLocalToParent.Y, DirectionToTargetLocalToParent.X));
    NewDesiredYaw = FRotator::NormalizeAxis(NewDesiredYaw);

    // For the client controlling this gun, update desired yaw directly for responsiveness
    APlayerController* PC = OwningPawn.IsValid() ? Cast<APlayerController>(OwningPawn->GetController()) : nullptr;
    if (PC && PC->IsLocalController())
    {
        if (FMath::Abs(FMath::FindDeltaAngleDegrees(DesiredGimbalRelativeYaw, NewDesiredYaw)) > 0.1f) // Only update if changed significantly
        {
            DesiredGimbalRelativeYaw = NewDesiredYaw; // Store the raw desired yaw
            Server_SetDesiredYaw(NewDesiredYaw); // Send to server
        }
    }
    else if (MyOwner->HasAuthority()) // AI or server-controlled aiming
    {
         if (FMath::Abs(FMath::FindDeltaAngleDegrees(DesiredGimbalRelativeYaw, NewDesiredYaw)) > 0.1f)
         {
            DesiredGimbalRelativeYaw = NewDesiredYaw; // Server directly sets its desired (will be clamped later)
         }
    }
    // Non-owning clients don't set DesiredGimbalRelativeYaw directly; they use the replicated CurrentActualGimbalRelativeYaw.
}

void USolaraqGimbalGunComponent::Server_SetDesiredYaw_Implementation(float NewDesiredYaw)
{
    // Server receives the desired yaw from the client.
    // It will clamp this yaw with constraints in its TickComponent before updating CurrentActualGimbalRelativeYaw.
    DesiredGimbalRelativeYaw = FRotator::NormalizeAxis(NewDesiredYaw);
}

void USolaraqGimbalGunComponent::OnRep_CurrentActualGimbalRelativeYaw()
{
    // Called on clients when CurrentActualGimbalRelativeYaw is replicated from the server.
    // The TickComponent will use this new value to smoothly update ClientVisualGimbalRelativeYaw
    // for remote clients. The owning client primarily drives its visuals from its own DesiredGimbalRelativeYaw
    // but this OnRep can serve as a correction mechanism if there's drift.
    // For simplicity, we let TickComponent handle the interpolation.
    // UE_LOG(LogSolaraqAI, Verbose, TEXT("Client %s: OnRep_CurrentActualGimbalRelativeYaw: %.2f"), *GetNameSafe(GetOwner()), CurrentActualGimbalRelativeYaw);
}

float USolaraqGimbalGunComponent::GetClampedRelativeYaw(float InYaw) const
{
    if (!bEnableYawConstraints)
    {
        return FRotator::NormalizeAxis(InYaw);
    }

    // Normalize the input yaw first to be relative to the constraint center
    float YawRelativeToCenter = FMath::FindDeltaAngleDegrees(ConstraintCenterRelativeYaw, InYaw);

    // Clamp this relative yaw to +/- MaxYawAngleFromCenter
    float ClampedYawRelativeToCenter = FMath::Clamp(YawRelativeToCenter, -MaxYawAngleFromCenter, MaxYawAngleFromCenter);

    // Convert back to yaw relative to parent's forward
    return FRotator::NormalizeAxis(ConstraintCenterRelativeYaw + ClampedYawRelativeToCenter);
}

#if WITH_EDITOR
void USolaraqGimbalGunComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

    if (PropertyName == GET_MEMBER_NAME_CHECKED(USolaraqGimbalGunComponent, bEnableYawConstraints) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(USolaraqGimbalGunComponent, ConstraintCenterRelativeYaw) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(USolaraqGimbalGunComponent, MaxYawAngleFromCenter))
    {
        // Force a redraw of the debug arc if constraint properties change
        // This is usually handled by the editor ticking, but explicit call can be useful.
        // For component debug rendering, it's often automatic when properties change if the component is selected.
    }
    if (PropertyName == GET_MEMBER_NAME_CHECKED(USolaraqGimbalGunComponent, MaxYawAngleFromCenter))
    {
        MaxYawAngleFromCenter = FMath::Clamp(MaxYawAngleFromCenter, 0.0f, 180.0f);
    }
     if (PropertyName == GET_MEMBER_NAME_CHECKED(USolaraqGimbalGunComponent, ConstraintCenterRelativeYaw))
    {
        ConstraintCenterRelativeYaw = FRotator::NormalizeAxis(ConstraintCenterRelativeYaw);
    }
}

void USolaraqGimbalGunComponent::RequestFire()
{
    if (CanFire())
    {
        // Client can play cosmetic effects immediately (muzzle flash, sound)
        // For example: UGameplayStatics::SpawnEmitterAttached(...) if you have a muzzle flash particle

        Server_PerformFire(); // Send request to server
    }
}

void USolaraqGimbalGunComponent::Server_PerformFire_Implementation()
{
    if (CanFire())
    {
        FireShot();
        LastFireTime = GetWorld()->GetTimeSeconds(); // Update last fire time on server
    }
}

bool USolaraqGimbalGunComponent::CanFire() const
{
    if (!ProjectileClass) return false;
    if (FireRate <= 0.f) return true; // Infinite fire rate if 0 or less

    return GetWorld()->GetTimeSeconds() >= LastFireTime + (1.0f / FireRate);
}

FTransform USolaraqGimbalGunComponent::GetMuzzleWorldTransform() const
{
    if (GunMeshComponent && MuzzleSocketName != NAME_None && GunMeshComponent->DoesSocketExist(MuzzleSocketName))
    {
        return GunMeshComponent->GetSocketTransform(MuzzleSocketName);
    }
    // Fallback to MuzzleOffset from the component's origin, taking current gimbal rotation into account.
    // The GunMeshComponent is already rotated by ClientVisualGimbalRelativeYaw.
    // We need the world rotation of this component + the visual yaw.
    
    FRotator GimbalWorldRotation = GetComponentRotation(); // Base rotation of the gimbal component itself
    // Add the GunMesh's relative (visual) yaw to the gimbal component's world yaw
    FRotator TotalEffectiveRotation = FRotator(GimbalWorldRotation.Pitch, FRotator::NormalizeAxis(GimbalWorldRotation.Yaw + ClientVisualGimbalRelativeYaw), GimbalWorldRotation.Roll);
    
    FVector WorldMuzzleLocation = GetComponentLocation() + TotalEffectiveRotation.RotateVector(MuzzleOffset);

    return FTransform(TotalEffectiveRotation, WorldMuzzleLocation);
}

void USolaraqGimbalGunComponent::FireShot()
{
    // --- Server-Side Execution Only ---
    AActor* MyOwner = GetOwner();
    if (!MyOwner || !MyOwner->HasAuthority() || !ProjectileClass) return;

    UWorld* const World = GetWorld();
    if (!World) return;

    FTransform MuzzleTransform = GetMuzzleWorldTransform();
    // Use the server's CurrentActualGimbalRelativeYaw for authoritative projectile direction.
    // The MuzzleTransform from GetMuzzleWorldTransform uses ClientVisualGimbalRelativeYaw which might be slightly ahead on the owning client.
    // For the server, we want the authoritative direction.
    FRotator AuthoritativeGimbalWorldRotation = GetComponentRotation();
    AuthoritativeGimbalWorldRotation.Yaw = FRotator::NormalizeAxis(AuthoritativeGimbalWorldRotation.Yaw + CurrentActualGimbalRelativeYaw);

    MuzzleTransform.SetRotation(AuthoritativeGimbalWorldRotation.Quaternion());
    // Recalculate location if MuzzleOffset was used, based on authoritative rotation
    if (MuzzleSocketName == NAME_None)
    {
        MuzzleTransform.SetLocation(GetComponentLocation() + AuthoritativeGimbalWorldRotation.RotateVector(MuzzleOffset));
    }


    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = OwningPawn.Get(); // Projectile is owned by the Pawn
    SpawnParams.Instigator = OwningPawn.Get(); // Pawn is the instigator
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ASolaraqProjectile* SpawnedProjectile = World->SpawnActor<ASolaraqProjectile>(ProjectileClass, MuzzleTransform.GetLocation(), MuzzleTransform.GetRotation().Rotator(), SpawnParams);

    if (SpawnedProjectile)
    {
        //UE_LOG(LogSolaraqCombat, Log, TEXT("Gimbal %s fired projectile %s"), *GetName(), *SpawnedProjectile->GetName());
        SpawnedProjectile->SetBaseDamage(BaseDamage); // Pass damage if projectile supports it

        // Set projectile velocity
        UProjectileMovementComponent* ProjMoveComp = SpawnedProjectile->FindComponentByClass<UProjectileMovementComponent>();
        if (ProjMoveComp)
        {
            // Combine ship velocity with muzzle velocity
            FVector OwnerVelocity = FVector::ZeroVector;
            if (OwningPawn.IsValid() && OwningPawn->GetMovementComponent())
            {
                OwnerVelocity = OwningPawn->GetMovementComponent()->Velocity;
            }
            else if (OwningPawn.IsValid() && OwningPawn->GetRootComponent() && OwningPawn->GetRootComponent()->IsSimulatingPhysics())
            {
                 UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(OwningPawn->GetRootComponent());
                 if(RootPrim) OwnerVelocity = RootPrim->GetPhysicsLinearVelocity();
            }


            const FVector MuzzleDirection = MuzzleTransform.GetRotation().GetForwardVector();
            ProjMoveComp->Velocity = OwnerVelocity + MuzzleDirection * ProjectileMuzzleSpeed;
            ProjMoveComp->UpdateComponentVelocity(); // Ensure component registers the change immediately
        }
    }
    else
    {
        UE_LOG(LogSolaraqCombat, Error, TEXT("Gimbal %s: Failed to spawn projectile!"), *GetName());
    }
}

void USolaraqGimbalGunComponent::DrawConstraintArc() const
{
    if (!bEnableYawConstraints || !GetOwner() || !GetWorld()) return;

    const float ArcRadius = 200.0f; // Visual size of the arc
    const int32 NumSegments = 24;  // Smoothness of the arc

    // Get parent's transform to make the arc relative to it
    FTransform ParentTransform = FTransform::Identity;
    if (GetAttachParent())
    {
        ParentTransform = GetAttachParent()->GetComponentTransform();
    }
    else
    {
        ParentTransform = GetOwner()->GetActorTransform();
    }
    
    // The gimbal component's location is the center of the arc
    FVector ArcCenterWorld = GetComponentLocation();

    // Calculate the min and max angles of the arc in world space
    float MinAngleDegRelative = FRotator::NormalizeAxis(ConstraintCenterRelativeYaw - MaxYawAngleFromCenter);
    float MaxAngleDegRelative = FRotator::NormalizeAxis(ConstraintCenterRelativeYaw + MaxYawAngleFromCenter);

    // The "forward" direction for the arc, based on parent's forward rotated by ConstraintCenterRelativeYaw
    FVector ArcForwardDirectionParent = ParentTransform.GetRotation().RotateVector(FRotator(0, ConstraintCenterRelativeYaw, 0).RotateVector(FVector::ForwardVector));
    ArcForwardDirectionParent.Z = 0; // Keep it on XY plane
    ArcForwardDirectionParent.Normalize();


    // Draw the arc itself
    DrawDebugCircleArc( // <--- CORRECT NAME
            GetWorld(),
            ArcCenterWorld,           // Center
            ArcRadius,                // Radius
            ArcForwardDirectionParent, // Direction vector for start of arc (or center, depending on interpretation)
            FMath::DegreesToRadians(MaxYawAngleFromCenter * 2.0f), // AngleWidth in Radians (total arc width)
            NumSegments,              // Segments
            FColor::Cyan,             // Color
            false,                    // PersistentLines
            -1.f,                     // Lifetime
            0,                        // DepthPriority
            2.f                       // Thickness
        );  

    // Draw lines for the boundaries of the arc
    FQuat ParentWorldQuat = ParentTransform.GetRotation();
    FVector LineStartWorld = ArcCenterWorld;

    FRotator MinRotRel(0, MinAngleDegRelative, 0);
    FVector MinDirWorld = ParentWorldQuat.RotateVector(MinRotRel.RotateVector(FVector::ForwardVector));
    MinDirWorld.Z = 0; MinDirWorld.Normalize(); // Flatten and normalize
    FVector LineEndMinWorld = ArcCenterWorld + MinDirWorld * ArcRadius;
    DrawDebugLine(GetWorld(), LineStartWorld, LineEndMinWorld, FColor::Yellow, false, -1.f, 0, 2.f);

    FRotator MaxRotRel(0, MaxAngleDegRelative, 0);
    FVector MaxDirWorld = ParentWorldQuat.RotateVector(MaxRotRel.RotateVector(FVector::ForwardVector));
    MaxDirWorld.Z = 0; MaxDirWorld.Normalize(); // Flatten and normalize
    FVector LineEndMaxWorld = ArcCenterWorld + MaxDirWorld * ArcRadius;
    DrawDebugLine(GetWorld(), LineStartWorld, LineEndMaxWorld, FColor::Yellow, false, -1.f, 0, 2.f);

    // Draw line for the center of the arc
    FRotator CenterRotRel(0, ConstraintCenterRelativeYaw, 0);
    FVector CenterDirWorld = ParentWorldQuat.RotateVector(CenterRotRel.RotateVector(FVector::ForwardVector));
    CenterDirWorld.Z = 0; CenterDirWorld.Normalize(); // Flatten and normalize
    FVector LineEndCenterWorld = ArcCenterWorld + CenterDirWorld * ArcRadius * 1.1f; // Slightly longer
    DrawDebugLine(GetWorld(), LineStartWorld, LineEndCenterWorld, FColor::Magenta, false, -1.f, 0, 2.f);
}
#endif // WITH_EDITOR