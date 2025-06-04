#include "Components/MiningLaserComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraFunctionLibrary.h" // If using Niagara
#include "NiagaraComponent.h"      // If using Niagara
#include "Sound/SoundBase.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "Components/AudioComponent.h"
#include "DrawDebugHelpers.h" // For debug line
#include "NiagaraSystem.h"
#include "Damage/MiningDamageType.h" // Our custom damage type
#include "Components/SceneComponent.h"
// #include "Logging/SolaraqLogChannels.h"

UMiningLaserComponent::UMiningLaserComponent() :
    MaxRange(5000.0f),
    DamagePerSecond(20.0f),
    MaxTurnRateDegreesPerSecond(90.0f),
    BeamParticleSystem(nullptr),
    ImpactParticleSystem(nullptr),
    ActiveLaserSound(nullptr),
    BeamTargetParameterName(TEXT("User.BeamTarget")), // Common Niagara user param name
    bLaserIsActive(false),
    LaserMuzzleComponent(nullptr),
    ActiveBeamCascadePSC(nullptr),
    ActiveBeamNiagaraComp(nullptr),
    ActiveImpactCascadePSC(nullptr),
    ActiveImpactNiagaraComp(nullptr),
    ActiveLaserAudioComponent(nullptr),
    CurrentTargetWorldLocation(FVector::ZeroVector),
    CurrentImpactPoint(FVector::ZeroVector),
    bCurrentlyHittingTarget(false)
{
    PrimaryComponentTick.bCanEverTick = true;
   PrimaryComponentTick.bStartWithTickEnabled = true; // Always tick to allow aiming/tracing even if effects are off

    // Ensure MiningDamageTypeClass is set to something valid by default in BP
    // For C++, you might load it:
    // static ConstructorHelpers::FClassFinder<UMiningDamageType> MiningDamageTypeClassFinder(TEXT("/Game/Blueprints/DamageTypes/DT_Mining")); // Adjust path
    // if (MiningDamageTypeClassFinder.Succeeded())
    // {
    //     MiningDamageTypeClass = MiningDamageTypeClassFinder.Class;
    // }
}

void UMiningLaserComponent::BeginPlay()
{
    Super::BeginPlay();

    // Attempt to find and set the LaserMuzzleComponent
    AActor* Owner = GetOwner();
    if (Owner)
    {
        // Priority 1: Find by specified FName LaserMuzzleComponentName
        if (LaserMuzzleComponentName != NAME_None)
        {
            TArray<USceneComponent*> SceneComponents;
            Owner->GetComponents<USceneComponent>(SceneComponents); // Get all scene components on the owner
            for (USceneComponent* SceneComp : SceneComponents)
            {
                if (SceneComp && SceneComp->GetFName() == LaserMuzzleComponentName)
                {
                    SetLaserMuzzleComponent(SceneComp);
                    UE_LOG(LogTemp, Log, TEXT("MiningLaserComponent '%s': Found and set LaserMuzzleComponent by name: '%s'"), *GetName(), *LaserMuzzleComponentName.ToString());
                    break;
                }
            }
            if (!LaserMuzzleComponent)
            {
                UE_LOG(LogTemp, Warning, TEXT("MiningLaserComponent '%s': LaserMuzzleComponentName '%s' was specified, but no component with that name was found on owner '%s'."), *GetName(), *LaserMuzzleComponentName.ToString(), *Owner->GetName());
            }
        }

        // Priority 2: If not found by name, try to find by socket (if LaserMuzzleComponent is still null)
        if (!LaserMuzzleComponent && !BeamSourceSocketName.IsNone())
        {
            TArray<UStaticMeshComponent*> MeshComponents;
            Owner->GetComponents<UStaticMeshComponent>(MeshComponents);
            for (UStaticMeshComponent* MeshComp : MeshComponents)
            {
                if (MeshComp->DoesSocketExist(BeamSourceSocketName))
                {
                    USceneComponent* SocketSceneComp = NewObject<USceneComponent>(Owner, TEXT("LaserMuzzleSocketAttachment"));
                    if (SocketSceneComp)
                    {
                        SocketSceneComp->AttachToComponent(MeshComp, FAttachmentTransformRules::KeepRelativeTransform, BeamSourceSocketName);
                        SocketSceneComp->RegisterComponent();
                        SetLaserMuzzleComponent(SocketSceneComp);
                        UE_LOG(LogTemp, Log, TEXT("MiningLaserComponent '%s': Attached muzzle to socket '%s' on '%s'"), *GetName(),*BeamSourceSocketName.ToString(), *MeshComp->GetName());
                        break;
                    }
                }
            }
            if (!LaserMuzzleComponent)
            {
                 UE_LOG(LogTemp, Warning, TEXT("MiningLaserComponent '%s': BeamSourceSocketName '%s' specified but not found on any StaticMeshComponent of owner '%s'."), *GetName(), *BeamSourceSocketName.ToString(), *Owner->GetName());
            }
        }

        // Priority 3: If still no muzzle, default to owner's root component (if LaserMuzzleComponent is still null)
        if (!LaserMuzzleComponent)
        {
            if (Owner->GetRootComponent())
            {
                SetLaserMuzzleComponent(Owner->GetRootComponent());
                UE_LOG(LogTemp, Log, TEXT("MiningLaserComponent '%s': Defaulted muzzle to owner's root component: '%s'"), *GetName(), *Owner->GetRootComponent()->GetName());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("MiningLaserComponent '%s': Owner '%s' has no RootComponent. Cannot set a default LaserMuzzleComponent."), *GetName(), *Owner->GetName());
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("MiningLaserComponent '%s' has no owner at BeginPlay!"), *GetName());
    }


    if (!MiningDamageTypeClass)
    {
        UE_LOG(LogTemp, Error, TEXT("MiningLaserComponent '%s': MiningDamageTypeClass is not set! Mining will not apply damage correctly."), *GetName());
    }

    if (!LaserMuzzleComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("MiningLaserComponent '%s': CRITICAL - LaserMuzzleComponent could not be resolved. Laser will not function correctly."), *GetName());
        SetComponentTickEnabled(false); // Disable tick if we can't get a muzzle
    }
}

void UMiningLaserComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopLaserEffects(true); 

    Super::EndPlay(EndPlayReason);
}

void UMiningLaserComponent::SetLaserMuzzleComponent(USceneComponent* Muzzle)
{
    if (Muzzle)
    {
        LaserMuzzleComponent = Muzzle;
        // UE_LOG(LogSolaraq, Log, TEXT("MiningLaserComponent %s: LaserMuzzleComponent set to %s"), *GetName(), *Muzzle->GetName());
    }
    else
    {
        // UE_LOG(LogSolaraq, Warning, TEXT("MiningLaserComponent %s: Attempted to set NULL LaserMuzzleComponent."), *GetName());
    }
}

FVector UMiningLaserComponent::GetLaserMuzzleLocation() const
{
    if (LaserMuzzleComponent)
    {
        return LaserMuzzleComponent->GetComponentLocation();
    }
    if (GetOwner())
    {
        return GetOwner()->GetActorLocation();
    }
    return FVector::ZeroVector;
}

FRotator UMiningLaserComponent::GetLaserMuzzleRotation() const
{
    if (LaserMuzzleComponent)
    {
        return LaserMuzzleComponent->GetComponentRotation();
    }
    if (GetOwner())
    {
        return GetOwner()->GetActorRotation();
    }
    return FRotator::ZeroRotator;
}

FVector UMiningLaserComponent::GetLaserMuzzleForwardVector() const
{
    if (LaserMuzzleComponent)
    {
        return LaserMuzzleComponent->GetForwardVector();
    }
    if (GetOwner())
    {
        return GetOwner()->GetActorForwardVector();
    }
    return FVector::ForwardVector;
}


void UMiningLaserComponent::ActivateLaser(bool bNewActiveState)
{
    if (bLaserIsActive == bNewActiveState)
    {
        return; // No change
    }

    bLaserIsActive = bNewActiveState;
   // ComponentTickEnabled is now true by default, this line is not strictly needed unless you want to disable tick when laser is off for other reasons.
   // For now, let's keep it ticking so aiming can update even if effects are briefly off.
   // SetComponentTickEnabled(bLaserIsActive || SomeOtherReasonToTick); 

    if (bLaserIsActive)
    {
        StartLaserEffects(); // Starts beam/impact effects
        if (GetOwner() && LaserMuzzleComponent) CurrentTargetWorldLocation = GetLaserMuzzleLocation() + GetLaserMuzzleForwardVector() * MaxRange * 0.5f;
    }
    else // Deactivating laser
    {
        StopLaserEffects(); // Stops beam/impact effects

    }
    UE_LOG(LogTemp, Log, TEXT("MiningLaserComponent: Laser Active State: %d"), bLaserIsActive);
}

void UMiningLaserComponent::SetTargetWorldLocation(const FVector& NewTargetLocation)
{
    CurrentTargetWorldLocation = NewTargetLocation;
}

void UMiningLaserComponent::StartLaserEffects()
{
    // Stop any existing effects first to prevent duplicates
    if (ActiveBeamCascadePSC) ActiveBeamCascadePSC->DestroyComponent();
    if (ActiveBeamNiagaraComp) ActiveBeamNiagaraComp->DestroyComponent();
    ActiveBeamCascadePSC = nullptr;
    ActiveBeamNiagaraComp = nullptr;

    if (BeamParticleSystem) // This is UParticleSystem*, which can be Cascade or Niagara
    {
        USceneComponent* ActualMuzzleComponent = LaserMuzzleComponent.Get(); // Get raw pointer
        USceneComponent* AttachParent = ActualMuzzleComponent ? ActualMuzzleComponent : (GetOwner() ? GetOwner()->GetRootComponent() : nullptr);
        FName AttachSocket = BeamSourceSocketName.IsNone() && LaserMuzzleComponent ? NAME_None : BeamSourceSocketName;

        if (!AttachParent) // Add a guard if GetOwner()->GetRootComponent() could also be null
        {
            UE_LOG(LogTemp, Error, TEXT("MiningLaserComponent: AttachParent is NULL in StartLaserEffects. Cannot spawn beam."));
            return;
        }
        
        if (UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(BeamParticleSystem))
        {
            ActiveBeamNiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
                NiagaraSystem,                      // SystemTemplate
                AttachParent,                       // AttachToComponent
                AttachSocket,                       // AttachPointName
                FVector::ZeroVector,                // Location (relative to attach point)
                FRotator::ZeroRotator,              // Rotation (relative to attach point)
                FVector::OneVector,                 // Scale (use OneVector for default 1,1,1 scale)
                EAttachLocation::KeepRelativeOffset,// LocationType
                true,                               // bAutoDestroy
                ENCPoolMethod::None,                // PoolingMethod
                true,                               // bAutoActivate (System activates immediately)
                true                                // bPreCullCheck (Typically true, allows system to be culled if off screen before first tick)
            );
            UE_LOG(LogTemp, Log, TEXT("MiningLaserComponent: Niagara Beam spawned: %s"), ActiveBeamNiagaraComp ? TEXT("Success") : TEXT("Failed"));
        }
        else if (UParticleSystem* CascadeSystem = Cast<UParticleSystem>(BeamParticleSystem)) // It's a Cascade UParticleSystem
        {
            ActiveBeamCascadePSC = UGameplayStatics::SpawnEmitterAttached(
                CascadeSystem, // Use the casted CascadeSystem here
                AttachParent,
                AttachSocket,
                FVector::ZeroVector,
                FRotator::ZeroRotator,
                EAttachLocation::KeepRelativeOffset,
                true
            );
            UE_LOG(LogTemp, Log, TEXT("MiningLaserComponent: Cascade Beam PSC spawned: %s"), ActiveBeamCascadePSC ? TEXT("Success") : TEXT("Failed"));
        }
    }

    if (ActiveLaserSound && !ActiveLaserAudioComponent)
    {
        // Similar explicit resolution for audio component attachment
        USceneComponent* AudioAttachToComponent = nullptr;
        if (LaserMuzzleComponent)
        {
            AudioAttachToComponent = LaserMuzzleComponent.Get();
        }
        else if (GetOwner())
        {
            AudioAttachToComponent = GetOwner()->GetRootComponent();
        }
        
        if (AudioAttachToComponent)
        {
            ActiveLaserAudioComponent = UGameplayStatics::SpawnSoundAttached(
                ActiveLaserSound,
                AudioAttachToComponent, // Use the resolved raw pointer
                NAME_None, // Sounds usually don't need a sub-socket if attached to the muzzle itself
                FVector::ZeroVector,
                EAttachLocation::KeepRelativeOffset,
                true
            );
            if (ActiveLaserAudioComponent)
            {
                ActiveLaserAudioComponent->Play();
                // UE_LOG(LogSolaraq, Log, TEXT("MiningLaserComponent: Laser sound started."));
            }
        }
        else
        {
            // UE_LOG(LogSolaraq, Error, TEXT("MiningLaserComponent: Cannot spawn laser sound, no valid attachment point."));
            UE_LOG(LogTemp, Error, TEXT("MiningLaserComponent: Cannot spawn laser sound, no valid attachment point."));
        }
    }
}

void UMiningLaserComponent::StopLaserEffects(bool bImmediate)
{
    if (ActiveBeamCascadePSC)
    {
        ActiveBeamCascadePSC->Deactivate();
        if (bImmediate) ActiveBeamCascadePSC->DestroyComponent();
        ActiveBeamCascadePSC = nullptr;
    }
    if (ActiveBeamNiagaraComp)
    {
        ActiveBeamNiagaraComp->Deactivate();
        if (bImmediate) ActiveBeamNiagaraComp->DestroyComponent(); // Niagara components also auto-destroy but can be forced
        ActiveBeamNiagaraComp = nullptr;
    }

    if (ActiveImpactCascadePSC)
    {
        ActiveImpactCascadePSC->Deactivate();
        if (bImmediate) ActiveImpactCascadePSC->DestroyComponent();
        ActiveImpactCascadePSC = nullptr;
    }
    if (ActiveImpactNiagaraComp)
    {
        ActiveImpactNiagaraComp->Deactivate();
        if (bImmediate) ActiveImpactNiagaraComp->DestroyComponent();
        ActiveImpactNiagaraComp = nullptr;
    }
    bCurrentlyHittingTarget = false;
    
    if (ActiveLaserAudioComponent)
    {
        ActiveLaserAudioComponent->Stop();
        ActiveLaserAudioComponent->DestroyComponent(); // Or fade out if desired
        ActiveLaserAudioComponent = nullptr;
        // UE_LOG(LogSolaraq, Log, TEXT("MiningLaserComponent: Laser sound stopped."));
    }
}

void UMiningLaserComponent::UpdateLaserAim(float DeltaTime)
{
    if (!LaserMuzzleComponent) return;

    const FVector MuzzleLocation = GetLaserMuzzleLocation();
    const FRotator CurrentMuzzleRotation = GetLaserMuzzleRotation();
    
    FVector DirectionToTarget = (CurrentTargetWorldLocation - MuzzleLocation).GetSafeNormal();
    if (DirectionToTarget.IsNearlyZero()) // Avoid issues if target is at muzzle
    {
        DirectionToTarget = LaserMuzzleComponent->GetForwardVector();
    }
    FRotator TargetMuzzleRotation = DirectionToTarget.Rotation();

    // Clamp rotation if attached to something that rotates itself (e.g. a turret base for the laser)
    // If LaserMuzzleComponent is directly on the ship and the ship rotates, this is fine.
    // If LaserMuzzleComponent is a child that should rotate independently:
    FRotator NewMuzzleRotation = FMath::RInterpTo(CurrentMuzzleRotation, TargetMuzzleRotation, DeltaTime, MaxTurnRateDegreesPerSecond);
    
    // If the laser muzzle is a child component that we want to rotate independently of its parent (the ship itself)
    // We need to set its World Rotation. If it's the root or just a socket, the owner handles rotation.
    // This assumes LaserMuzzleComponent is something we can directly rotate, like a dedicated SceneComponent for the laser.
    LaserMuzzleComponent->SetWorldRotation(NewMuzzleRotation);
}


void UMiningLaserComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
   // We still want to update aim even if the laser effects are not active,
   // so the player controller can get the correct target for its widget.
   // Effects themselves (beam, sound, damage) are only applied if bLaserIsActive is true.
   if (!GetOwner() || !GetWorld() || !LaserMuzzleComponent)
    {
        // Check both types of beam and impact effects
        if (ActiveBeamCascadePSC || ActiveBeamNiagaraComp || ActiveImpactCascadePSC || ActiveImpactNiagaraComp)
        {
            StopLaserEffects(true); 
        }
        return;
    }
   
    // 1. Update Laser Aim (rotate the LaserMuzzleComponent towards CurrentTargetWorldLocation)
    UpdateLaserAim(DeltaTime);

   // Only do trace, damage, and visual effects if the laser is actually active
   if (bLaserIsActive)
   {
       // 2. Perform Line Trace
       FHitResult HitResult;
       FVector TraceStart = GetLaserMuzzleLocation();
       FVector TraceEnd = TraceStart + GetLaserMuzzleForwardVector() * MaxRange;
       CurrentImpactPoint = TraceEnd; // Default if nothing is hit
       bCurrentlyHittingTarget = false;

       FCollisionQueryParams CollisionParams;
       CollisionParams.AddIgnoredActor(GetOwner());
       AActor* OwnerOwner = GetOwner()->GetOwner(); // If laser is on a turret owned by a ship
       if(OwnerOwner) CollisionParams.AddIgnoredActor(OwnerOwner);



       if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, CollisionParams)) // Consider ECC_Destructible if you make one
       {
           CurrentImpactPoint = HitResult.ImpactPoint;
           bCurrentlyHittingTarget = true;
           ApplyMiningDamage(DeltaTime, HitResult);
           // UE_LOG(LogTemp, Verbose, TEXT("MiningLaser hit %s at %s"), *HitResult.GetActor()->GetName(), *HitResult.ImpactPoint.ToString());
       }
       else
       {
           // UE_LOG(LogTemp, Verbose, TEXT("MiningLaser hit nothing, endpoint %s"), *TraceEnd.ToString());
       }


       // 3. Update Visuals (Beam and Impact)
       UpdateLaserBeamVisuals(TraceStart, CurrentImpactPoint, bCurrentlyHittingTarget);
       UpdateImpactEffect(HitResult, bCurrentlyHittingTarget); // Pass full HitResult
   }
   else // Laser not active, ensure effects are off
   {
       if (ActiveBeamCascadePSC || ActiveBeamNiagaraComp || ActiveImpactCascadePSC || ActiveImpactNiagaraComp)
       {
           StopLaserEffects(false); // Gentle stop
       }
       bCurrentlyHittingTarget = false;
       CurrentImpactPoint = GetLaserMuzzleLocation() + GetLaserMuzzleForwardVector() * MaxRange; // Still update for potential queries
   }

    // For Debugging
    // DrawDebugLine(GetWorld(), TraceStart, CurrentImpactPoint, FColor::Red, false, -1, 0, 1.f);
}

void UMiningLaserComponent::UpdateLaserBeamVisuals(const FVector& BeamStart, const FVector& BeamEnd, bool bHitSomething)
{
    if (ActiveBeamNiagaraComp && !BeamTargetParameterName.IsNone())
    {
        // BeamStart is where the Niagara component is attached (muzzle)
        // We need to give it the BeamEnd location in the local space of the Niagara component
        FVector LocalBeamEnd = ActiveBeamNiagaraComp->GetComponentTransform().InverseTransformPosition(BeamEnd);
        ActiveBeamNiagaraComp->SetVectorParameter(BeamTargetParameterName, LocalBeamEnd);
    }
    else if (ActiveBeamCascadePSC) // For Cascade
    {
        // Common method for Cascade beams using source/target points
        ActiveBeamCascadePSC->SetBeamSourcePoint(0, BeamStart, 0); 
        ActiveBeamCascadePSC->SetBeamTargetPoint(0, BeamEnd, 0);   
    }
}

void UMiningLaserComponent::UpdateImpactEffect(const FHitResult& HitResult, bool bIsHitting)
{
    if (!ImpactParticleSystem) // This is UParticleSystem*, which can be Cascade or Niagara
    {
        // If there's no template, ensure any active impact effects are stopped
        if (ActiveImpactCascadePSC)
        {
            ActiveImpactCascadePSC->Deactivate();
            // ActiveImpactCascadePSC->DestroyComponent(); ActiveImpactCascadePSC = nullptr; // Or immediate
        }
        if (ActiveImpactNiagaraComp)
        {
            ActiveImpactNiagaraComp->Deactivate();
            // ActiveImpactNiagaraComp->DestroyComponent(); ActiveImpactNiagaraComp = nullptr; // Or immediate
        }
        return;
    }

    if (bIsHitting)
    {
        if (UNiagaraSystem* NiagaraImpactSystem = Cast<UNiagaraSystem>(ImpactParticleSystem))
        {
            if (!ActiveImpactNiagaraComp) // If no active Niagara impact, spawn one
            {
                ActiveImpactNiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                    GetWorld(),
                    NiagaraImpactSystem,
                    HitResult.ImpactPoint,
                    HitResult.ImpactNormal.Rotation()
                );
                UE_LOG(LogTemp, Log, TEXT("MiningLaserComponent: Niagara Impact spawned at %s"), *HitResult.ImpactPoint.ToString());
            }
            else // Niagara impact exists, update it
            {
                ActiveImpactNiagaraComp->SetWorldLocationAndRotation(HitResult.ImpactPoint, HitResult.ImpactNormal.Rotation());
                if (!ActiveImpactNiagaraComp->IsActive()) ActiveImpactNiagaraComp->ActivateSystem(true);
            }
            // Deactivate any Cascade impact if Niagara is now active
            if (ActiveImpactCascadePSC)
            {
                ActiveImpactCascadePSC->Deactivate();
                // ActiveImpactCascadePSC->DestroyComponent(); ActiveImpactCascadePSC = nullptr; // Or immediate
            }
        }
        else if (UParticleSystem* CascadeImpactSystem = Cast<UParticleSystem>(ImpactParticleSystem)) // It's a Cascade UParticleSystem
        {
            if (!ActiveImpactCascadePSC) // If no active Cascade impact, spawn one
            {
                ActiveImpactCascadePSC = UGameplayStatics::SpawnEmitterAtLocation(
                    GetWorld(),
                    CascadeImpactSystem,
                    HitResult.ImpactPoint,
                    HitResult.ImpactNormal.Rotation()
                );
                UE_LOG(LogTemp, Log, TEXT("MiningLaserComponent: Cascade Impact spawned at %s"), *HitResult.ImpactPoint.ToString());
            }
            else // Cascade impact exists, update it
            {
                ActiveImpactCascadePSC->SetWorldLocationAndRotation(HitResult.ImpactPoint, HitResult.ImpactNormal.Rotation());
                if(!ActiveImpactCascadePSC->IsActive()) ActiveImpactCascadePSC->ActivateSystem(true);
            }
            // Deactivate any Niagara impact if Cascade is now active
            if (ActiveImpactNiagaraComp)
            {
                ActiveImpactNiagaraComp->Deactivate();
                // ActiveImpactNiagaraComp->DestroyComponent(); ActiveImpactNiaraComp = nullptr; // Or immediate
            }
        }
    }
    else // Not hitting anything
    {
        if (ActiveImpactCascadePSC)
        {
            ActiveImpactCascadePSC->Deactivate();
            UE_LOG(LogTemp, Log, TEXT("MiningLaserComponent: Cascade Impact PSC deactivated (no hit)."));
        }
        if (ActiveImpactNiagaraComp)
        {
            ActiveImpactNiagaraComp->Deactivate();
            UE_LOG(LogTemp, Log, TEXT("MiningLaserComponent: Niagara Impact Comp deactivated (no hit)."));
        }
    }
}


void UMiningLaserComponent::ApplyMiningDamage(float DeltaTime, const FHitResult& HitResult)
{
    if (!bCurrentlyHittingTarget || !HitResult.GetActor() || DamagePerSecond <= 0.f || !MiningDamageTypeClass)
    {
        return;
    }

    AActor* HitActor = HitResult.GetActor();
    AController* OwnerController = nullptr;
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (OwnerPawn)
    {
        OwnerController = OwnerPawn->GetController();
    }

    float DamageToApply = DamagePerSecond * DeltaTime;

    // UE_LOG(LogSolaraq, Verbose, TEXT("Applying %.2f mining damage to %s"), DamageToApply, *HitActor->GetName());
    UGameplayStatics::ApplyPointDamage(
        HitActor,
        DamageToApply,
        GetLaserMuzzleForwardVector(), // Direction of damage
        HitResult,                  // Full hit result for more info (impact point, normal)
        OwnerController,            // Instigating controller
        GetOwner(),                 // Damage causer (the actor owning this component)
        MiningDamageTypeClass       // Our custom damage type
    );
}
