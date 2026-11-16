#include "ReporterGameMode.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
FSlateFontInfo UiFont(const UFont* Source, float Scale, float LayoutScale)
{
    const UFont* Font = Source ? Source : GEngine->GetMediumFont();
    const FSlateFontInfo Legacy = Font->GetLegacySlateFontInfo();
    const float Size = FMath::Clamp(Legacy.Size * Scale / LayoutScale, 1.0f, 512.0f);
    if (Font->GetCompositeFont())
    {
        FSlateFontInfo Result = Legacy;
        Result.Size = Size;
        return Result;
    }
    return FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
}
}

class FReporterTextLayer
{
public:
    struct FLabel
    {
        TSharedPtr<STextBlock> Widget;
        SConstraintCanvas::FSlot* Slot = nullptr;
        FString Value;
        FSlateFontInfo Font;
        FLinearColor Color = FLinearColor::Transparent;
    };

    TSharedRef<SConstraintCanvas> Panel = SNew(SConstraintCanvas)
        .Visibility(EVisibility::HitTestInvisible)
        .Clipping(EWidgetClipping::ClipToBounds);
    TArray<FLabel> Labels;
    FVector2D ViewportSize = FVector2D::UnitVector;
    float LayoutScale = 1.0f;
    int32 Used = 0;
    int32 PreviouslyVisible = 0;
};

void AReporterHUD::BeginTextFrame()
{
    if (!UiTextLayer)
    {
        UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
        if (!Viewport || !FSlateApplication::IsInitialized())
        {
            return;
        }
        UiTextLayer = MakeShared<FReporterTextLayer>();
        // Editable story fields use layer 50 and keep their native hit testing.
        Viewport->AddViewportWidgetContent(UiTextLayer->Panel, 40);
    }

    FReporterTextLayer& Layer = *UiTextLayer;
    Layer.Used = 0;
    Layer.ViewportSize = FVector2D(FMath::Max(1.0f, Canvas->ClipX), FMath::Max(1.0f, Canvas->ClipY));
    const FGeometry& Geometry = Layer.Panel->GetCachedGeometry();
    Layer.LayoutScale = FMath::Max(0.1f, Geometry.GetAccumulatedLayoutTransform().GetScale());
}

void AReporterHUD::EndTextFrame()
{
    if (UiTextLayer)
    {
        FReporterTextLayer& Layer = *UiTextLayer;
        for (int32 Index = Layer.Used; Index < Layer.PreviouslyVisible; ++Index)
        {
            Layer.Labels[Index].Widget->SetVisibility(EVisibility::Collapsed);
        }
        Layer.PreviouslyVisible = Layer.Used;
    }
}

void AReporterHUD::RemoveTextLayer()
{
    if (UiTextLayer)
    {
        if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
        {
            Viewport->RemoveViewportWidgetContent(UiTextLayer->Panel);
        }
        UiTextLayer.Reset();
    }
}

void AReporterHUD::DrawUiText(const FString& Value, FLinearColor Color, float X, float Y, UFont* Font, float Scale)
{
    if (!UiTextLayer || Value.IsEmpty() || ((bSettingsOpen || TitlePage == ETitlePage::Load) && !bDrawingSettings))
    {
        return;
    }

    FReporterTextLayer& Layer = *UiTextLayer;
    const int32 Index = Layer.Used++;
    if (!Layer.Labels.IsValidIndex(Index))
    {
        auto& NewLabel = Layer.Labels.AddDefaulted_GetRef();
        NewLabel.Widget = SNew(STextBlock);
        Layer.Panel->AddSlot()
            .Expose(NewLabel.Slot)
            .Alignment(FVector2D::ZeroVector)
            .AutoSize(true)
            [NewLabel.Widget.ToSharedRef()];
    }

    auto& Label = Layer.Labels[Index];
    const FVector2D Anchor = FVector2D(FMath::RoundToFloat(X), FMath::RoundToFloat(Y)) / Layer.ViewportSize;
    const FAnchors Anchors(Anchor.X, Anchor.Y);
    if (Label.Slot->GetAnchors().Minimum != Anchors.Minimum)
    {
        Label.Slot->SetAnchors(Anchors);
    }
    const FSlateFontInfo ResolvedFont = UiFont(Font, Scale, Layer.LayoutScale);
    if (Label.Font != ResolvedFont)
    {
        Label.Font = ResolvedFont;
        Label.Widget->SetFont(ResolvedFont);
    }
    if (Label.Value != Value)
    {
        Label.Value = Value;
        Label.Widget->SetText(FText::FromString(Value));
    }
    if (Label.Color != Color)
    {
        Label.Color = Color;
        Label.Widget->SetColorAndOpacity(FSlateColor(Color));
    }
    if (Index >= Layer.PreviouslyVisible)
    {
        Label.Widget->SetVisibility(EVisibility::HitTestInvisible);
    }
}

void AReporterHUD::MeasureUiText(const FString& Value, float& Width, float& Height, UFont* Font, float Scale) const
{
    if (!UiTextLayer)
    {
        Width = Height = 0.0f;
        return;
    }

    const float LayoutScale = UiTextLayer->LayoutScale;
    const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const FVector2D Extent = Measure->Measure(Value, UiFont(Font, Scale, LayoutScale), LayoutScale);
    Width = Extent.X;
    Height = Extent.Y;
}
