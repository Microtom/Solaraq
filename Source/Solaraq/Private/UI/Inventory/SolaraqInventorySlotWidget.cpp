// SolaraqInventorySlotWidget.cpp

#include "UI/Inventory/SolaraqInventorySlotWidget.h" // Adjust path as needed
#include "Components/Image.h"

void USolaraqInventorySlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// In the editor, this will make sure our configuration function runs
	// so we can see a default state. We can just configure it as a default 1x1 empty slot.
	if (IsDesignTime())
	{
		ConfigureSlotAppearance(true, true, true, true);
	}
}

void USolaraqInventorySlotWidget::ConfigureSlotAppearance(bool bIsTopEdge, bool bIsRightEdge, bool bIsBottomEdge, bool bIsLeftEdge)
{
	// Ensure the textures have been assigned in the Blueprint.
	if (!CornerPieceTexture || !StraightPieceTexture)
	{
		return;
	}

	// Determine if each corner is an "outer" corner of the item's shape.
	const bool bIsTopLeftCorner = bIsTopEdge && bIsLeftEdge;
	const bool bIsTopRightCorner = bIsTopEdge && bIsRightEdge;
	const bool bIsBottomRightCorner = bIsBottomEdge && bIsRightEdge;
	const bool bIsBottomLeftCorner = bIsBottomEdge && bIsLeftEdge;

	// This logic can be simplified. We configure each of the four UImage components.
	// Visibility is controlled by setting a null brush.

	// --- Configure Top-Left Image ---
	if (bIsTopLeftCorner)
	{
		Corner_TL->SetBrushFromTexture(CornerPieceTexture);
		Corner_TL->SetRenderTransformAngle(0.f);
	}
	else if (bIsTopEdge)
	{
		Corner_TL->SetBrushFromTexture(StraightPieceTexture);
		Corner_TL->SetRenderTransformAngle(0.f); // Assumes straight piece is a top border
	}
	else if (bIsLeftEdge)
	{
		Corner_TL->SetBrushFromTexture(StraightPieceTexture);
		Corner_TL->SetRenderTransformAngle(-90.f);
	}
	else // This is an internal slot piece
	{
		Corner_TL->SetBrush(FSlateBrush()); // Set to an empty brush to make it invisible
	}

	// --- Configure Top-Right Image ---
	if (bIsTopRightCorner)
	{
		Corner_TR->SetBrushFromTexture(CornerPieceTexture);
		Corner_TR->SetRenderTransformAngle(90.f);

	}
	else if (bIsTopEdge)
	{
		Corner_TR->SetBrushFromTexture(StraightPieceTexture);
		Corner_TR->SetRenderTransformAngle(0.f);
	}
	else if (bIsRightEdge)
	{
		Corner_TR->SetBrushFromTexture(StraightPieceTexture);
		Corner_TR->SetRenderTransformAngle(90.f);
	}
	else
	{
		Corner_TR->SetBrush(FSlateBrush());
	}
	
	// --- Configure Bottom-Right Image ---
	if (bIsBottomRightCorner)
	{
		Corner_BR->SetBrushFromTexture(CornerPieceTexture);
		Corner_BR->SetRenderTransformAngle(180.f);
	}
	else if (bIsBottomEdge)
	{
		Corner_BR->SetBrushFromTexture(StraightPieceTexture);
		Corner_BR->SetRenderTransformAngle(180.f);
	}
	else if (bIsRightEdge)
	{
		Corner_BR->SetBrushFromTexture(StraightPieceTexture);
		Corner_BR->SetRenderTransformAngle(90.f);
	}
	else
	{
		Corner_BR->SetBrush(FSlateBrush());
	}

	// --- Configure Bottom-Left Image ---
	if (bIsBottomLeftCorner)
	{
		Corner_BL->SetBrushFromTexture(CornerPieceTexture);
		Corner_BL->SetRenderTransformAngle(-90.f);
	}
	else if (bIsBottomEdge)
	{
		Corner_BL->SetBrushFromTexture(StraightPieceTexture);
		Corner_BL->SetRenderTransformAngle(180.f);
	}
	else if (bIsLeftEdge)
	{
		Corner_BL->SetBrushFromTexture(StraightPieceTexture);
		Corner_BL->SetRenderTransformAngle(-90.f);
	}
	else
	{
		Corner_BL->SetBrush(FSlateBrush());
	}
}