#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MinimapWidgetBase.generated.h"

class UCanvasPanel;
class UImage;
class UMinimapComponent;
class UMinimapTrackableComponent;

// Structure to hold pooled icon data
USTRUCT(BlueprintType)
struct FMinimapIconData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
    TObjectPtr<UImage> IconWidget = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
    bool bIsInUse = false;

    FMinimapIconData() = default;
    FMinimapIconData(UImage* InWidget) : IconWidget(InWidget), bIsInUse(false) {}
};


UCLASS()
class YOURPROJECTNAME_API UMinimapWidgetBase : public UUserWidget
{
    GENERATED_BODY()

protected:
    // --- Bindings (Set these in the derived UMG Blueprint) ---

    // The CanvasPanel where icons will be placed. MUST bind this in UMG!
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UCanvasPanel> IconCanvas;

    // The Image widget representing the player icon. MUST bind this in UMG!
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UImage> PlayerIcon;

    // Optional: Background Image
    // UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    // TObjectPtr<UImage> BackgroundImage;

    // --- Configuration (Can be set in Blueprint Defaults) ---

    // The size of the minimap widget itself (used for scaling calculations).
    // Important: This should match the actual size of the IconCanvas!
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    FVector2D MinimapSize = FVector2D(256.0f, 256.0f);

    // --- Internal State ---

    // Pool of icon widgets to reuse
    UPROPERTY(Transient)
    TArray<FMinimapIconData> IconPool;

    // Cached reference to the Minimap Component
    UPROPERTY(Transient)
    TWeakObjectPtr<UMinimapComponent> MinimapComp;

    // --- Overrides ---
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // --- Helper Functions ---

    // Gets or creates an icon widget from the pool.
    UImage* GetOrCreateIconWidget();

    // Updates the position, appearance, and visibility of all icons.
    void UpdateMinimapIcons();

    // Converts world coordinates to local minimap coordinates.
    // Returns true if the point is within the map radius, false otherwise.
    UFUNCTION(BlueprintPure, Category = "Minimap|Internal")
    bool WorldToMapCoordinates(const FVector& PlayerLocationProjected, const FVector& TargetLocationProjected, float MapRadius, out FVector2D& MapCoordinates);

    // Sort trackables by draw priority
    void SortTrackables(TArray<UMinimapTrackableComponent*>& ValidTrackables);
};