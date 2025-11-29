// SolaraqInventorySlotWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SolaraqInventorySlotWidget.generated.h"

// Forward declare the component we need
class UImage;

/**
 * Represents a single, procedurally drawn tile in the inventory grid.
 * Its appearance is configured by the parent grid based on its context.
 */
UCLASS()
class SOLARAQ_API USolaraqInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Configures the appearance of this slot's borders based on its context within the grid.
	 * This is the main function called by the inventory grid widget.
	 * @param bIsTopEdge - True if this slot forms a top border for an item.
	 * @param bIsRightEdge - True if this slot forms a right border for an item.
	 * @param bIsBottomEdge - True if this slot forms a bottom border for an item.
	 * @param bIsLeftEdge - True if this slot forms a left border for an item.
	 */
	void ConfigureSlotAppearance(bool bIsTopEdge, bool bIsRightEdge, bool bIsBottomEdge, bool bIsLeftEdge);

protected:
	// This function is where you can assign your textures from the Blueprint child class.
	virtual void NativePreConstruct() override;
	
	// --- UPROPERTY Bindings ---
	// These pointers will be automatically linked to the UMG widgets with the same name.
	// Ensure your UMG widgets in WBP_InventorySlot are named EXACTLY this.

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Corner_TL;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Corner_TR;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Corner_BR;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Corner_BL;

	// --- Blueprint-Assignable Textures ---
	// These properties will appear in the Details panel of your WBP_InventorySlot.

	// The L-shaped corner piece with two borders.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solaraq|Appearance")
	TObjectPtr<UTexture2D> CornerPieceTexture;

	// The straight piece with one border.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solaraq|Appearance")
	TObjectPtr<UTexture2D> StraightPieceTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solaraq|Appearance")
	TObjectPtr<UTexture2D> FillPieceTexture;
};