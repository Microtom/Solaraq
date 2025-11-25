#include "UI/SolaraqMinimapWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet/GameplayStatics.h"
#include "UI/SolaraqMinimapIcon.h"
#include "Logging/SolaraqLogChannels.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Environment/CelestialBodyBase.h"
#include "Environment/SolaraqSatellite.h"

void USolaraqMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!MapContainer) return;

    // ... Player setup & UpdateTimer logic ...
    FVector PlayerLoc = FVector::ZeroVector;
    APawn* PlayerPawn = GetOwningPlayerPawn();
    if (PlayerPawn) PlayerLoc = PlayerPawn->GetActorLocation();
    
    UpdateTimer += InDeltaTime;
    if (UpdateTimer > 0.5f) { RefreshTrackedActors(); UpdateTimer = 0.0f; }

    FVector2D WidgetSize = MyGeometry.GetLocalSize();
    FVector2D Center = WidgetSize * 0.5f;
    
    // Determine the radius of the widget (assuming it's a square/circle)
    float WidgetRadius = WidgetSize.X * 0.5f;
    float MapScale = WidgetRadius / RadarRange; 

    for (int32 i = ActiveIcons.Num() - 1; i >= 0; --i)
    {
        FMinimapIconData& Data = ActiveIcons[i];
        AActor* Actor = Data.OwnerActor.Get();

        // ... Validity checks ...
        bool bShouldRemove = false;
        if (!Actor) bShouldRemove = true;
        else if (Actor->Implements<USolaraqMinimapInterface>())
        {
             FSolaraqMinimapData MapData = ISolaraqMinimapInterface::Execute_GetMinimapData(Actor);
             if (!MapData.bIsVisible) bShouldRemove = true;
        }
        if (bShouldRemove) {
            if (Data.IconWidget) Data.IconWidget->RemoveFromParent();
            ActiveIcons.RemoveAt(i);
            continue;
        }

        // --- POSITION & BOUNDS LOGIC ---

        FVector Delta = Actor->GetActorLocation() - PlayerLoc;
        float MapX = Delta.Y * MapScale; 
        float MapY = Delta.X * MapScale * -1.0f; 

        FVector2D IconPos(MapX, MapY);
        float DistFromCenter = IconPos.Size();

        // Check if the icon is outside the radar radius
        if (DistFromCenter > WidgetRadius)
        {
            // CASE A: Hide it (It's out of range)
            Data.IconWidget->SetVisibility(ESlateVisibility::Hidden);
        }
        else
        {
            // It is in range, make sure it is visible
            Data.IconWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
            
            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Data.IconWidget->Slot))
            {
                CanvasSlot->SetPosition(Center + IconPos);
            }
            
            // Apply rotation only if visible
            Data.IconWidget->SetRenderTransformAngle(Actor->GetActorRotation().Yaw);
        }
    }
}

void USolaraqMinimapWidget::RefreshTrackedActors()
{
    // 1. Find all actors that implement the Minimap Interface
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsWithInterface(GetWorld(), USolaraqMinimapInterface::StaticClass(), FoundActors);

    // Optional: Log total count (good for debugging, comment out later to reduce spam)
    // UE_LOG(LogSolaraqMinimap, Warning, TEXT("Minimap Scan: Found %d actors."), FoundActors.Num());
    
    for (AActor* Actor : FoundActors)
    {
        // 2. Check if we are already tracking this actor
        bool bIsTracked = false;
        for (const auto& IconData : ActiveIcons)
        {
            if (IconData.OwnerActor == Actor) 
            {
                bIsTracked = true;
                break;
            }
        }

        // 3. If NOT tracked, try to add it
        if (!bIsTracked && IconTemplateClass)
        {
            // --- NEW: Retrieve all data in one struct call ---
            FSolaraqMinimapData MapData = ISolaraqMinimapInterface::Execute_GetMinimapData(Actor);

            // Optimization: If the actor is hidden (e.g. cloaked ship, dead), don't create a widget yet.
            if (!MapData.bIsVisible)
            {
                continue; 
            }

            // 4. Create the Widget
            if (USolaraqMinimapIcon* NewIcon = CreateWidget<USolaraqMinimapIcon>(this, IconTemplateClass))
            {
                MapContainer->AddChild(NewIcon);
                
                // 5. Center the icon so rotation happens around the middle
                if (UCanvasPanelSlot* NewCanvasSlot = Cast<UCanvasPanelSlot>(NewIcon->Slot))
                {
                    NewCanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                    NewCanvasSlot->SetAutoSize(true);
                }

                // 6. Clamp Scale (Prevents microscopic Moons or massive Suns covering the map)
                // Adjust these numbers based on your preference.
                float ClampedScale = FMath::Clamp(MapData.IconScale, 0.1f, 2.5f);

                // 7. Configure the Icon Visuals
                NewIcon->SetupIcon(MapData.IconType, MapData.IconColor, ClampedScale);

                // 8. Add to our tracking array
                FMinimapIconData NewIconData;
                NewIconData.OwnerActor = Actor;
                NewIconData.IconWidget = NewIcon;
                ActiveIcons.Add(NewIconData);

                // Log Success
                UE_LOG(LogSolaraqMinimap, Warning, TEXT("Minimap: Spawning Icon for %s (Type: %d, Scale: %.2f)"), 
                    *Actor->GetName(), (int32)MapData.IconType, ClampedScale);
            }
        }
    }
}
FVector2D USolaraqMinimapWidget::WorldToMapCoords(FVector WorldLoc, FVector2D WidgetSize)
{
    // Center of the map widget
    FVector2D Center = WidgetSize * 0.5f;

    // Normalize World Location (-WorldSize to +WorldSize becomes -1 to 1)
    // Assuming 0,0,0 is the center of your level
    float NormX = WorldLoc.X / MapWorldSize;
    float NormY = WorldLoc.Y / MapWorldSize;

    // Scale to Widget Size
    // Mapping: World X -> Widget Y (Inverted), World Y -> Widget X
    float MapX = Center.X + (NormY * Center.X); 
    float MapY = Center.Y - (NormX * Center.Y); 

    return FVector2D(MapX, MapY);
}