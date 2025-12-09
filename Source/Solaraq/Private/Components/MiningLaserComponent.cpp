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
#include "Logging/SolaraqLogChannels.h"
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
    MiningDamageTypeClass = UMiningDamageType::StaticClass();
}

void UMiningLaserComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (Owner)
    {
        // Priority 1: Find by specified FName LaserMuzzleComponentName
        if (LaserMuzzleComponentName != NAME_None)
        {
            TArray<USceneComponent*> SceneComponents;
            Owner->GetComponents<USceneComponent>(SceneComponents); 
            for (USceneComponent* SceneComp : SceneComponents)
            {
                if (SceneComp && SceneComp->GetFName() == LaserMuzzleComponentName)
                {
                    SetLaserMuzzleComponent(SceneComp);
                    UE_LOG(LogSolaraqMining, Log, TEXT("MiningLaserComponent: Found and set LaserMuzzleComponent by name: '%s'"), *LaserMuzzleComponentName.ToString());
                    break;
                }
            }
            if (!LaserMuzzleComponent)
            {
                UE_LOG(LogSolaraqMining, Warning, TEXT("MiningLaserComponent: LaserMuzzleComponentName '%s' specified but not found."), *LaserMuzzleComponentName.ToString());
            }
        }

        // Priority 2: If not found by name, try to find by socket 
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
                        UE_LOG(LogSolaraqMining, Log, TEXT("MiningLaserComponent: Attached muzzle to socket '%s'."), *BeamSourceSocketName.ToString());
                        break;
                    }
                }
            }
        }

        // --- NEW PRIORITY 3: Auto-detect the standard C++ Mount ---
        if (!LaserMuzzleComponent)
        {
            TArray<USceneComponent*> SceneComponents;
            Owner->GetComponents<USceneComponent>(SceneComponents);
            for (USceneComponent* SceneComp : SceneComponents)
            {
                // Check specifically for the component name created in SolaraqShipBase constructor
                if (SceneComp && SceneComp->GetFName() == FName("MiningLaserMount"))
                {
                    SetLaserMuzzleComponent(SceneComp);
                    UE_LOG(LogSolaraqMining, Log, TEXT("MiningLaserComponent: Auto-detected standard 'MiningLaserMount'."));
                    break;
                }
            }
        }
        // -----------------------------------------------------------

        // Priority 4: Final Failure Check
        if (!LaserMuzzleComponent)
        {
            UE_LOG(LogSolaraqMining, Error, TEXT("MiningLaserComponent '%s': No Muzzle found! Please create a SceneComponent named 'MiningLaserMount' on the ship."), *GetName());
            // This disables the logic, which is why your laser wasn't moving or hitting anything
            SetComponentTickEnabled(false); 
            return;
        }
    }
    else
    {
        UE_LOG(LogSolaraqMining, Error, TEXT("MiningLaserComponent '%s' has no owner at BeginPlay!"), *GetName());
    }

    if (!MiningDamageTypeClass)
    {
        UE_LOG(LogSolaraqMining, Error, TEXT("MiningLaserComponent '%s': MiningDamageTypeClass is not set!"), *GetName());
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
        return; 
    }

    bLaserIsActive = bNewActiveState;

    if (bLaserIsActive)
    {
        StartLaserEffects(); 
        if (GetOwner() && LaserMuzzleComponent) CurrentTargetWorldLocation = GetLaserMuzzleLocation() + GetLaserMuzzleForwardVector() * MaxRange * 0.5f;
    }
    else 
    {
        StopLaserEffects(); 

    }
    UE_LOG(LogSolaraqMining, Log, TEXT("MiningLaserComponent: Laser Active State: %d"), bLaserIsActive);
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

    if (BeamParticleSystem) 
    {
        USceneComponent* ActualMuzzleComponent = LaserMuzzleComponent.Get();
        USceneComponent* AttachParent = ActualMuzzleComponent ? ActualMuzzleComponent : (GetOwner() ? GetOwner()->GetRootComponent() : nullptr);
        FName AttachSocket = BeamSourceSocketName.IsNone() && LaserMuzzleComponent ? NAME_None : BeamSourceSocketName;

        if (!AttachParent) 
        {
            UE_LOG(LogSolaraqMining, Error, TEXT("MiningLaserComponent: AttachParent is NULL in StartLaserEffects."));
            return;
        }
        
        if (UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(BeamParticleSystem))
        {
            ActiveBeamNiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
                NiagaraSystem, AttachParent, AttachSocket, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector,
                EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::None, true, true
            );
            UE_LOG(LogSolaraqMining, Verbose, TEXT("MiningLaserComponent: Niagara Beam spawned."));
        }
        else if (UParticleSystem* CascadeSystem = Cast<UParticleSystem>(BeamParticleSystem)) 
        {
            ActiveBeamCascadePSC = UGameplayStatics::SpawnEmitterAttached(
                CascadeSystem, AttachParent, AttachSocket, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true
            );
            UE_LOG(LogSolaraqMining, Verbose, TEXT("MiningLaserComponent: Cascade Beam PSC spawned."));
        }
    }

    if (ActiveLaserSound && !ActiveLaserAudioComponent)
    {
        USceneComponent* AudioAttachToComponent = LaserMuzzleComponent ? LaserMuzzleComponent.Get() : (GetOwner() ? GetOwner()->GetRootComponent() : nullptr);
        if (AudioAttachToComponent)
        {
            ActiveLaserAudioComponent = UGameplayStatics::SpawnSoundAttached(
                ActiveLaserSound, AudioAttachToComponent, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true
            );
            if (ActiveLaserAudioComponent) ActiveLaserAudioComponent->Play();
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
        if (bImmediate) ActiveBeamNiagaraComp->DestroyComponent(); 
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
        ActiveLaserAudioComponent->DestroyComponent(); 
        ActiveLaserAudioComponent = nullptr;
    }
}

void UMiningLaserComponent::UpdateLaserAim(float DeltaTime)
{
    if (!LaserMuzzleComponent)
    {
        static float LastErrorLog = 0.0f;
        float Now = GetWorld()->GetTimeSeconds();
        if(Now - LastErrorLog > 2.0f) 
        {
            UE_LOG(LogSolaraqMining, Error, TEXT("UpdateLaserAim Failed: LaserMuzzleComponent is NULL!"));
            LastErrorLog = Now;
        }
        return;
    }

    const FVector MuzzleLocation = LaserMuzzleComponent->GetComponentLocation();
    const FRotator CurrentMuzzleRotation = LaserMuzzleComponent->GetComponentRotation();
    
    // Debugging Variables
    static float LastAimLogTime = 0.0f;
    float CurrentTime = GetWorld()->GetTimeSeconds();
    bool bShouldLog = (CurrentTime - LastAimLogTime > 0.5f);

    FVector DirectionToTarget = (CurrentTargetWorldLocation - MuzzleLocation).GetSafeNormal();
    
    if (DirectionToTarget.IsNearlyZero()) 
    {
        DirectionToTarget = LaserMuzzleComponent->GetForwardVector();
        if (bShouldLog) UE_LOG(LogSolaraqMining, Warning, TEXT("UpdateLaserAim: Target is at Muzzle location! Using Forward."));
    }

    FRotator TargetMuzzleRotation = DirectionToTarget.Rotation();

    FRotator NewMuzzleRotation = FMath::RInterpTo(CurrentMuzzleRotation, TargetMuzzleRotation, DeltaTime, MaxTurnRateDegreesPerSecond);
    
    LaserMuzzleComponent->SetWorldRotation(NewMuzzleRotation);

    if (bShouldLog)
    {
        UE_LOG(LogSolaraqMining, Verbose, TEXT("UpdateLaserAim: CurRot: %s -> TargetRot: %s"), 
            *CurrentMuzzleRotation.ToString(), *TargetMuzzleRotation.ToString());
        LastAimLogTime = CurrentTime;
    }

    // VISUAL DEBUG
    FVector CurrentForward = LaserMuzzleComponent->GetForwardVector();
    DrawDebugLine(GetWorld(), MuzzleLocation, MuzzleLocation + CurrentForward * 1000.0f, FColor::Red, false, -1.0f, 0, 3.0f);
    DrawDebugLine(GetWorld(), MuzzleLocation, MuzzleLocation + DirectionToTarget * 1000.0f, FColor::Blue, false, -1.0f, 0, 1.0f);
}

void UMiningLaserComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
   if (!GetOwner() || !GetWorld() || !LaserMuzzleComponent)
    {
        if (ActiveBeamCascadePSC || ActiveBeamNiagaraComp || ActiveImpactCascadePSC || ActiveImpactNiagaraComp)
        {
            StopLaserEffects(true); 
        }
        return;
    }
   
    // 1. Update Laser Aim
    UpdateLaserAim(DeltaTime);

   if (bLaserIsActive)
   {
       // 2. Perform Line Trace
       FHitResult HitResult;
       FVector TraceStart = GetLaserMuzzleLocation();
       FVector TraceEnd = TraceStart + GetLaserMuzzleForwardVector() * MaxRange;
       
       CurrentImpactPoint = TraceEnd; 
       bCurrentlyHittingTarget = false;

       FCollisionQueryParams CollisionParams;
       CollisionParams.AddIgnoredActor(GetOwner());
       AActor* OwnerOwner = GetOwner()->GetOwner(); 
       if(OwnerOwner) CollisionParams.AddIgnoredActor(OwnerOwner);

       bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, CollisionParams);

       if (bHit)
       {
           CurrentImpactPoint = HitResult.ImpactPoint;
           bCurrentlyHittingTarget = true;
           
           AActor* HitActor = HitResult.GetActor();
           if (HitActor)
           {
               // Log every 0.5 seconds to avoid spamming
               static float LastHitLog = 0.0f;
               float Now = GetWorld()->GetTimeSeconds();
               if(Now - LastHitLog > 0.5f)
               {
                   UE_LOG(LogSolaraqMining, Log, TEXT("Trace HIT: %s | Comp: %s | PhysMat: %s"), 
                       *HitActor->GetName(), 
                       *HitResult.GetComponent()->GetName(),
                       HitResult.PhysMaterial.IsValid() ? *HitResult.PhysMaterial->GetName() : TEXT("None"));
                   LastHitLog = Now;
               }
           }

           DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 10.0f, FColor::Red, false, -1.0f);
           DrawDebugLine(GetWorld(), TraceStart, HitResult.ImpactPoint, FColor::Red, false, -1.0f, 0, 1.0f);

           ApplyMiningDamage(DeltaTime, HitResult);
       }
       else
       {
           DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Red, false, -1.0f, 0, 0.5f);
       }

       // 3. Update Visuals
       UpdateLaserBeamVisuals(TraceStart, CurrentImpactPoint, bCurrentlyHittingTarget);
       UpdateImpactEffect(HitResult, bCurrentlyHittingTarget);
   }
   else 
   {
       if (ActiveBeamCascadePSC || ActiveBeamNiagaraComp || ActiveImpactCascadePSC || ActiveImpactNiagaraComp)
       {
           StopLaserEffects(false); 
       }
       bCurrentlyHittingTarget = false;
       CurrentImpactPoint = GetLaserMuzzleLocation() + GetLaserMuzzleForwardVector() * MaxRange; 
   }
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
    if (!ImpactParticleSystem) 
    {
        if (ActiveImpactCascadePSC) ActiveImpactCascadePSC->Deactivate();
        if (ActiveImpactNiagaraComp) ActiveImpactNiagaraComp->Deactivate();
        return;
    }

    if (bIsHitting)
    {
        if (UNiagaraSystem* NiagaraImpactSystem = Cast<UNiagaraSystem>(ImpactParticleSystem))
        {
            if (!ActiveImpactNiagaraComp) 
            {
                ActiveImpactNiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                    GetWorld(), NiagaraImpactSystem, HitResult.ImpactPoint, HitResult.ImpactNormal.Rotation()
                );
                UE_LOG(LogSolaraqMining, Verbose, TEXT("MiningLaserComponent: Niagara Impact spawned at %s"), *HitResult.ImpactPoint.ToString());
            }
            else 
            {
                ActiveImpactNiagaraComp->SetWorldLocationAndRotation(HitResult.ImpactPoint, HitResult.ImpactNormal.Rotation());
                if (!ActiveImpactNiagaraComp->IsActive()) ActiveImpactNiagaraComp->ActivateSystem(true);
            }
            if (ActiveImpactCascadePSC) ActiveImpactCascadePSC->Deactivate();
        }
        else if (UParticleSystem* CascadeImpactSystem = Cast<UParticleSystem>(ImpactParticleSystem)) 
        {
            if (!ActiveImpactCascadePSC) 
            {
                ActiveImpactCascadePSC = UGameplayStatics::SpawnEmitterAtLocation(
                    GetWorld(), CascadeImpactSystem, HitResult.ImpactPoint, HitResult.ImpactNormal.Rotation()
                );
                UE_LOG(LogSolaraqMining, Verbose, TEXT("MiningLaserComponent: Cascade Impact spawned at %s"), *HitResult.ImpactPoint.ToString());
            }
            else 
            {
                ActiveImpactCascadePSC->SetWorldLocationAndRotation(HitResult.ImpactPoint, HitResult.ImpactNormal.Rotation());
                if(!ActiveImpactCascadePSC->IsActive()) ActiveImpactCascadePSC->ActivateSystem(true);
            }
            if (ActiveImpactNiagaraComp) ActiveImpactNiagaraComp->Deactivate();
        }
    }
    else // Not hitting anything
    {
        if (ActiveImpactCascadePSC) ActiveImpactCascadePSC->Deactivate();
        if (ActiveImpactNiagaraComp) ActiveImpactNiagaraComp->Deactivate();
    }
}


void UMiningLaserComponent::ApplyMiningDamage(float DeltaTime, const FHitResult& HitResult)
{
    if (!bCurrentlyHittingTarget || !HitResult.GetActor() || DamagePerSecond <= 0.f)
    {
        return;
    }

    // Safety check just in case it was explicitly nulled out
    if (!MiningDamageTypeClass)
    {
        // Error already logged in BeginPlay
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

    UGameplayStatics::ApplyPointDamage(
        HitActor,
        DamageToApply,
        GetLaserMuzzleForwardVector(), 
        HitResult,                 
        OwnerController,           
        GetOwner(),                 
        MiningDamageTypeClass      
    );

    // --- DEBUG LOGGING ---
    // Log once per second to confirm damage is flowing
    static float LastDamageLog = 0.0f;
    float Now = GetWorld()->GetTimeSeconds();
    if(Now - LastDamageLog > 1.0f)
    {
        UE_LOG(LogSolaraqMining, Log, TEXT("Applied %.2f mining damage to %s using type %s"), 
            DamageToApply, *HitActor->GetName(), *MiningDamageTypeClass->GetName());
        LastDamageLog = Now;
    }
}