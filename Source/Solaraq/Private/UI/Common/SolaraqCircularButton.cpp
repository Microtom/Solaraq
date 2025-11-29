// SolaraqCircularButton.cpp
#include "UI/Common/SolaraqCircularButton.h"
#include "Widgets/Input/SButton.h"
#include "Components/ButtonSlot.h" // Required for building the slot

// Define a custom Slate Button class locally to handle the geometry logic
class SSolaraqCircularButton : public SButton
{
public:
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (IsPointInCircle(MyGeometry, MouseEvent))
		{
			return SButton::OnMouseButtonDown(MyGeometry, MouseEvent);
		}
		// If outside the circle, treat it as if the button wasn't clicked
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (IsPointInCircle(MyGeometry, MouseEvent))
		{
			return SButton::OnMouseButtonDoubleClick(MyGeometry, MouseEvent);
		}
		return FReply::Unhandled();
	}
    
    // Optional: Handle Up event filtering if strictly needed, though Down is usually sufficient for "Clicks"
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
        // We usually let Up pass through if Down was captured, but strict checking:
		if (IsPointInCircle(MyGeometry, MouseEvent))
		{
			return SButton::OnMouseButtonUp(MyGeometry, MouseEvent);
		}
		return FReply::Unhandled();
	}

private:
	bool IsPointInCircle(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
	{
		// 1. Get Local Space coordinates of the mouse
		const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		const FVector2D LocalSize = MyGeometry.GetLocalSize();

		// 2. Calculate Center and Radius
		const FVector2D Center = LocalSize * 0.5f;
		const float Radius = FMath::Min(LocalSize.X, LocalSize.Y) * 0.5f;

		// 3. Check Distance Squared (Optimization to avoid Sqrt)
		const float DistSquared = FVector2D::DistSquared(LocalPos, Center);
		const float RadiusSquared = Radius * Radius;

		return DistSquared <= RadiusSquared;
	}
};

TSharedRef<SWidget> USolaraqCircularButton::RebuildWidget()
{
	// Create the custom slate widget
	TSharedRef<SSolaraqCircularButton> NewButton = SNew(SSolaraqCircularButton)
		.OnClicked(BIND_UOBJECT_DELEGATE(FOnClicked, SlateHandleClicked))
		.OnPressed(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandlePressed))
		.OnReleased(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandleReleased))
		.OnHovered_UObject(this, &ThisClass::SlateHandleHovered)
		.OnUnhovered_UObject(this, &ThisClass::SlateHandleUnhovered)
		.ButtonStyle(&WidgetStyle)
		.ClickMethod(ClickMethod)
		.TouchMethod(TouchMethod)
		.PressMethod(PressMethod)
		.IsFocusable(IsFocusable);

	// Assign it to the parent class member 'MyButton' so UButton internals work correctly
	MyButton = NewButton;

	// Handle child content (like text or images inside the button)
	if (GetChildrenCount() > 0)
	{
		Cast<UButtonSlot>(GetContentSlot())->BuildSlot(NewButton);
	}

	return NewButton;
}