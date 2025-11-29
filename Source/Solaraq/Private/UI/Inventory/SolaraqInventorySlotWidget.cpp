// SolaraqInventorySlotWidget.cpp
#include "UI/Inventory/SolaraqInventorySlotWidget.h"
#include "Components/Image.h"

void USolaraqInventorySlotWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    
    // Ensure the slot itself doesn't block drag-and-drop events meant for the Grid
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    if (IsDesignTime())
    {
        ConfigureSlotAppearance(true, true, true, true);
    }
}

void USolaraqInventorySlotWidget::ConfigureSlotAppearance(bool bIsTopEdge, bool bIsRightEdge, bool bIsBottomEdge, bool bIsLeftEdge)
{
    if (!CornerPieceTexture || !StraightPieceTexture || !FillPieceTexture) return;

    auto ConfigureQuadrant = [&](UImage* Image, bool bVerticalBorder, bool bHorizontalBorder, float Angle)
    {
        Image->SetRenderTransformAngle(Angle);
        Image->SetVisibility(ESlateVisibility::Visible);

        if (bVerticalBorder && bHorizontalBorder)
        {
            Image->SetBrushFromTexture(CornerPieceTexture);
        }
        else if (bVerticalBorder)
        {
            Image->SetBrushFromTexture(StraightPieceTexture);
        }
        else if (bHorizontalBorder)
        {
            // Assuming StraightTexture is Top-oriented
            Image->SetBrushFromTexture(StraightPieceTexture); 
        }
        else
        {
            Image->SetBrushFromTexture(FillPieceTexture);
        }
    };

    // --- Top Left ---
    if (bIsTopEdge && bIsLeftEdge) { Corner_TL->SetBrushFromTexture(CornerPieceTexture); Corner_TL->SetRenderTransformAngle(0.f); }
    else if (bIsTopEdge)           { Corner_TL->SetBrushFromTexture(StraightPieceTexture); Corner_TL->SetRenderTransformAngle(0.f); }
    else if (bIsLeftEdge)          { Corner_TL->SetBrushFromTexture(StraightPieceTexture); Corner_TL->SetRenderTransformAngle(-90.f); }
    else                           { Corner_TL->SetBrushFromTexture(FillPieceTexture);     Corner_TL->SetRenderTransformAngle(0.f); }

    // --- Top Right ---
    if (bIsTopEdge && bIsRightEdge) { Corner_TR->SetBrushFromTexture(CornerPieceTexture); Corner_TR->SetRenderTransformAngle(90.f); }
    else if (bIsTopEdge)            { Corner_TR->SetBrushFromTexture(StraightPieceTexture); Corner_TR->SetRenderTransformAngle(0.f); } 
    else if (bIsRightEdge)          { Corner_TR->SetBrushFromTexture(StraightPieceTexture); Corner_TR->SetRenderTransformAngle(90.f); }
    else                            { Corner_TR->SetBrushFromTexture(FillPieceTexture);     Corner_TR->SetRenderTransformAngle(0.f); }

    // --- Bottom Right ---
    if (bIsBottomEdge && bIsRightEdge) { Corner_BR->SetBrushFromTexture(CornerPieceTexture); Corner_BR->SetRenderTransformAngle(180.f); }
    else if (bIsBottomEdge)            { Corner_BR->SetBrushFromTexture(StraightPieceTexture); Corner_BR->SetRenderTransformAngle(180.f); }
    else if (bIsRightEdge)             { Corner_BR->SetBrushFromTexture(StraightPieceTexture); Corner_BR->SetRenderTransformAngle(90.f); }
    else                               { Corner_BR->SetBrushFromTexture(FillPieceTexture);     Corner_BR->SetRenderTransformAngle(0.f); }

    // --- Bottom Left ---
    if (bIsBottomEdge && bIsLeftEdge) { Corner_BL->SetBrushFromTexture(CornerPieceTexture); Corner_BL->SetRenderTransformAngle(270.f); }
    else if (bIsBottomEdge)           { Corner_BL->SetBrushFromTexture(StraightPieceTexture); Corner_BL->SetRenderTransformAngle(180.f); }
    else if (bIsLeftEdge)             { Corner_BL->SetBrushFromTexture(StraightPieceTexture); Corner_BL->SetRenderTransformAngle(270.f); }
    else                              { Corner_BL->SetBrushFromTexture(FillPieceTexture);     Corner_BL->SetRenderTransformAngle(0.f); }
}