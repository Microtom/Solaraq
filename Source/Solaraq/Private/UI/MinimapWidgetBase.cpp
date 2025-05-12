#include "UI/MinimapWidgetBase.h" // Adjust path
#include "UI/MinimapComponent.h"  // Adjust path
#include "UI/MinimapTrackableComponent.h" // Adjust path
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetTree.h" // For creating widgets dynamically

void UMinimapWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();

    // Ensure required widgets are bound
    if (!IconCanvas)
    {
        // UE_LOG(LogMinimap, Error, TEXT("MinimapWidgetBase: IconCanvas is not bound!"));
        SetVisibility(ESlateVisibility::Collapsed); // Hide if setup is wrong
        return;
    }
     if (!PlayerIcon)
    {
        // UE_LOG(LogMinimap, Error, TEXT("MinimapWidgetBase: PlayerIcon is not bound!"));
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }


    // Find the MinimapComponent on the owning Player Controller
    APlayerController* PC = GetOwningPlayerController();
    if (PC)
    {
        MinimapComp = PC->FindComponentByClass<UMinimapComponent>();
        if (!MinimapComp.IsValid())
        {
           // UE_LOG(LogMinimap, Warning, TEXT("MinimapWidgetBase could not find MinimapComponent on Owning Player Controller."));
            SetVisibility(ESlateVisibility::Collapsed);
        }
    }
     else
     {
         // This can happen briefly during PIE startup/shutdown
         // UE_LOG(LogMinimap, Warning, TEXT("MinimapWidgetBase: Owning Player Controller is null during NativeConstruct."));
         SetVisibility(ESlateVisibility::Collapsed); // Hide until controller is valid
     }
}

void UMinimapWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!MinimapComp.IsValid() || !IconCanvas || !PlayerIcon)
    {
        // Attempt to find the component again if it was missing initially
        if(!MinimapComp.IsValid())
        {
             APlayerController* PC = GetOwningPlayerController();
             if(PC) MinimapComp = PC->FindComponentByClass<UMinimapComponent>();
        }

        // If still invalid or widgets missing, do nothing
        if (!MinimapComp.IsValid() || !IconCanvas || !PlayerIcon)
        {
             SetVisibility(ESlateVisibility::Hidden); // Keep hidden if core parts missing
             return;
        }
         else
         {
             SetVisibility(ESlateVisibility::Visible); // Make visible if found
         }

    }

    // Ensure MinimapSize reflects the actual canvas size if desired (or keep configured value)
    // MinimapSize = IconCanvas->GetCachedGeometry().GetLocalSize(); // Option: Use actual size at runtime


    // --- Update Player Icon ---
    FRotator PlayerRot = MinimapComp->GetPlayerPawnRotation();
    PlayerIcon->SetRenderTransformAngle(PlayerRot.Yaw); // Rotate player icon based on Yaw

    // Center the player icon (adjust offset if needed)
    UCanvasPanelSlot* PlayerSlot = Cast<UCanvasPanelSlot>(PlayerIcon->Slot);
    if (PlayerSlot)
    {
        PlayerSlot->SetPosition(MinimapSize * 0.5f); // Center it
        PlayerSlot->SetAlignment(FVector2D(0.5f, 0.5f)); // Set alignment to center pivot
    }

    // --- Update Tracked Icons ---
    UpdateMinimapIcons();
}

void UMinimapWidgetBase::UpdateMinimapIcons()
{
    if (!MinimapComp.IsValid()) return;

    // Reset use status for all pooled icons
    for (FMinimapIconData& Data : IconPool)
    {
        Data.bIsInUse = false;
    }

    const TArray<TWeakObjectPtr<UMinimapTrackableComponent>>& TrackedList = MinimapComp->GetTrackedComponents();
    const FVector PlayerLocProjected = MinimapComp->GetPlayerPawnLocationProjected();
    const float MapRadius = MinimapComp->MapWorldRadius;

    // Collect valid trackables first
    TArray<UMinimapTrackableComponent*> ValidTrackables;
    ValidTrackables.Reserve(TrackedList.Num());
    for (const TWeakObjectPtr<UMinimapTrackableComponent>& WeakTrackable : TrackedList)
    {
        if (WeakTrackable.IsValid())
        {
            ValidTrackables.Add(WeakTrackable.Get());
        }
    }

    // Sort by priority (optional but good for overlap control)
    SortTrackables(ValidTrackables);

    // --- Process Each Valid Trackable ---
    for (UMinimapTrackableComponent* Trackable : ValidTrackables)
    {
        if (!Trackable || Trackable->GetOwner() == MinimapComp->GetOwner()) // Skip if invalid or is the player pawn itself
        {
            continue;
        }

        FVector TargetLoc = Trackable->GetTrackedActorLocation();
        TargetLoc.Z = MinimapComp->ProjectionZ; // Project onto the plane

        FVector2D MapCoord;
        if (WorldToMapCoordinates(PlayerLocProjected, TargetLoc, MapRadius, MapCoord))
        {
             // Check visibility radius if set
             if (Trackable->VisibilityRadius > 0.0f)
             {
                 float DistSq = FVector::DistSquaredXY(PlayerLocProjected, TargetLoc);
                 if (DistSq > FMath::Square(Trackable->VisibilityRadius))
                 {
                     continue; // Outside visibility radius, don't draw
                 }
             }

            // Get an icon widget from the pool
            UImage* IconWidget = GetOrCreateIconWidget();
            if (!IconWidget) continue; // Should not happen if creation works

            // Configure the icon
            IconWidget->SetBrush(Trackable->MinimapIcon);
            IconWidget->SetColorAndOpacity(Trackable->IconColor);
            // Size can be set by the brush or overridden
            FVector2D FinalIconSize = Trackable->MinimapIcon.ImageSize;
             if(FinalIconSize.IsZero()) // Use component size if brush size is zero
             {
                FinalIconSize = Trackable->IconSize;
             }


            // Set position and alignment in the canvas
            UCanvasPanelSlot* IconSlot = Cast<UCanvasPanelSlot>(IconWidget->Slot);
            if (IconSlot)
            {
                IconSlot->SetPosition(MapCoord);
                IconSlot->SetSize(FinalIconSize);
                IconSlot->SetAlignment(FVector2D(0.5f, 0.5f)); // Center pivot
                IconSlot->SetZOrder(Trackable->DrawPriority); // Set draw order
            }

            // Set rotation if needed
            if (Trackable->bTrackRotation)
            {
                IconWidget->SetRenderTransformAngle(Trackable->GetTrackedActorRotation().Yaw);
            }
            else
            {
                IconWidget->SetRenderTransformAngle(0.0f);
            }

            IconWidget->SetVisibility(ESlateVisibility::HitTestInvisible); // Make it visible
        }
        // Else: The target is outside the map radius.
        // Icon remains hidden (or implement clamping logic here).
    }


    // Hide unused icons from the pool
    for (FMinimapIconData& Data : IconPool)
    {
        if (!Data.bIsInUse && Data.IconWidget)
        {
            Data.IconWidget->SetVisibility(ESlateVisibility::Collapsed); // Use Collapsed to remove from layout
        }
    }
}


UImage* UMinimapWidgetBase::GetOrCreateIconWidget()
{
    // Try to find an unused widget in the pool
    for (FMinimapIconData& Data : IconPool)
    {
        if (!Data.bIsInUse && Data.IconWidget)
        {
            Data.bIsInUse = true;
            return Data.IconWidget;
        }
    }

    // If no unused widget found, create a new one
    if(WidgetTree && IconCanvas) // Ensure WidgetTree and Canvas are valid
    {
        UImage* NewIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        if (NewIcon)
        {
            IconCanvas->AddChild(NewIcon); // Add to the canvas
            FMinimapIconData NewData(NewIcon);
            NewData.bIsInUse = true;
            IconPool.Add(NewData);
             // UE_LOG(LogMinimap, Verbose, TEXT("Created new minimap icon. Pool size: %d"), IconPool.Num());
            return NewIcon;
        }
        else
        {
             // UE_LOG(LogMinimap, Error, TEXT("Failed to construct new UImage for minimap icon pool!"));
        }
    }
     else
     {
          // UE_LOG(LogMinimap, Error, TEXT("Cannot create minimap icon: WidgetTree or IconCanvas is null!"));
     }

    return nullptr; // Failed to get or create
}


bool UMinimapWidgetBase::WorldToMapCoordinates(const FVector& PlayerLocationProjected, const FVector& TargetLocationProjected, float MapRadius, out FVector2D& MapCoordinates)
{
    if (MapRadius <= 0.0f) return false;

    const FVector Delta = TargetLocationProjected - PlayerLocationProjected;
    const float DistanceSquared = Delta.SizeSquared2D(); // Use 2D distance

    if (DistanceSquared > FMath::Square(MapRadius))
    {
        // Optional: Implement clamping here if desired
        // If bClampToEdge is true, calculate intersection with the circle
        return false; // Outside map radius
    }

    // Normalize the delta vector relative to the map radius
    // Treat map radius as the extent from center (0,0) to edge (1,0) or (0,1)
    const FVector2D NormalizedOffset(Delta.X / MapRadius, Delta.Y / MapRadius); // [-1, 1] range

    // Convert normalized offset [-1, 1] to map coordinates [0, MinimapSize]
    // Map center corresponds to NormalizedOffset (0,0)
    // MapCoord = (NormalizedOffset * 0.5f + 0.5f) * MinimapSize;
    // But since player icon is at center (MinimapSize * 0.5), we can simplify:
    MapCoordinates = (MinimapSize * 0.5f) + (NormalizedOffset * MinimapSize * 0.5f);


    return true; // Inside map radius
}

void UMinimapWidgetBase::SortTrackables(TArray<UMinimapTrackableComponent*>& ValidTrackables)
{
     ValidTrackables.Sort([](const UMinimapTrackableComponent& A, const UMinimapTrackableComponent& B) {
         return A.DrawPriority < B.DrawPriority; // Lower priority drawn first (behind)
     });
}