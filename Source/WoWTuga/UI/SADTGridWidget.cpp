#include "SADTGridWidget.h"
#include "../BobBuilder.h"
#include "Rendering/DrawElements.h"

void SADTGridWidget::Construct(const FArguments& InArgs) { Loader = InArgs._OwnerLoader; }

int32 SADTGridWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    if (!Loader) return LayerId;
    float TileS = AllottedGeometry.GetLocalSize().X / 64.0f;

    for (int32 i = 0; i < 4096; ++i) {
        FLinearColor Color = FLinearColor(0.05f, 0.05f, 0.05f);
        if (Loader->TileMap[i] == ETileState::Available) Color = FLinearColor::Blue;
        else if (Loader->TileMap[i] == ETileState::Selected) Color = FLinearColor::Red;
        else if (Loader->TileMap[i] == ETileState::Loaded) Color = FLinearColor::Green;

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId,
            AllottedGeometry.ToPaintGeometry(
                FVector2f(TileS - 1.0f, TileS - 1.0f), // Tamanho do quadrado
                FSlateLayoutTransform(FVector2f((i % 64) * TileS, (i / 64) * TileS)) // Posição
            ),
            FAppStyle::GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            Color
        );
    }
    return LayerId + 1;
}

FReply SADTGridWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) {
    FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    float TileS = MyGeometry.GetLocalSize().X / 64.0f;
    int32 X = FMath::FloorToInt(LocalPos.X / TileS);
    int32 Y = FMath::FloorToInt(LocalPos.Y / TileS);
    if (X >= 0 && X < 64 && Y >= 0 && Y < 64) Loader->ToggleTileSelection(Y * 64 + X);
    return FReply::Handled();
}