#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ABobBuilder;

class SADTGridWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SADTGridWidget) : _OwnerLoader(nullptr) {}
        SLATE_ARGUMENT(ABobBuilder*, OwnerLoader)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
    ABobBuilder* Loader;
};