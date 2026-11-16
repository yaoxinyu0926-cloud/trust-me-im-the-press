#include "ReporterGameMode.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"

namespace
{
const FVector2D DesignSize(1600, 900);
const FLinearColor Ink(0.045f, 0.027f, 0.016f);
const FLinearColor Teal(0.018f, 0.11f, 0.10f);
const FLinearColor Red(0.23f, 0.034f, 0.027f);
const FLinearColor FormPaper(0.97f, 0.86f, 0.66f);
const FLinearColor Pale(0.97f, 0.91f, 0.76f);
const FLinearColor Muted(0.37f, 0.31f, 0.24f);
const FBox2D MaleButton({150, 717}, {299, 768});
const FBox2D FemaleButton({310, 717}, {470, 768});
const FBox2D NameField({150, 643}, {470, 694});
const FBox2D RollButton({575, 455}, {815, 508});
const FBox2D BeginButton({1190, 772}, {1468, 829});
const TCHAR* SkillNames[] = {TEXT("Interviewing"), TEXT("Investigation"), TEXT("Editing")};
const TCHAR* TalentNames[] = {TEXT("Frugal"), TEXT("Business Instinct"), TEXT("Good Ear")};
const TCHAR* TalentDescriptions[] = {
    TEXT("Pay 15% less for food,\nfurniture, and equipment.\nRent stays the same."),
    TEXT("Earn 15% more from\npublished stories at any\nreporting angle."),
    TEXT("Find an extra story tip\nin a local conversation\nonce per day.")
};

FBox2D TalentCard(int32 Index)
{
    const double X = 575 + 299 * Index;
    return FBox2D({X, 589}, {X + 285, 739});
}
}

void AReporterHUD::RollReporterSkills()
{
    for (int32& Skill : ReporterSkills)
    {
        Skill = FMath::FRand() < 0.25f ? FMath::RandRange(21, 40) : FMath::RandRange(1, 20);
    }
}

int32 AReporterHUD::GetFoodPrice(int32 BasePrice) const
{
    return ReporterTalent == EReporterTalent::Frugal
        ? FMath::RoundToInt(BasePrice * 0.85) : BasePrice;
}

void AReporterHUD::BeginReporterCreation()
{
    TitlePage = ETitlePage::CreateReporter;
    OpeningComicPage = nullptr;
    OpeningComicArt.Reset();
    ReporterName = TEXT("Alex Reed");
    ReporterRollsLeft = 10;
    bFemaleReporter = false;
    ReporterTalent = EReporterTalent::Frugal;
    RollReporterSkills();

    const FString Art = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt/ReporterCreationDesk.jpg");
    ReporterCreationDesk = FImageUtils::ImportFileAsTexture2D(Art);
}

void AReporterHUD::RemoveReporterCreationControls()
{
    if (ReporterCreationControls)
    {
        if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
        {
            Viewport->RemoveViewportWidgetContent(ReporterCreationControls.ToSharedRef());
        }
        ReporterCreationControls.Reset();
    }
}

void AReporterHUD::DrawReporterCreation()
{
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    if (View.X <= 0 || View.Y <= 0)
    {
        return;
    }
    const double S = FMath::Min(View.X / DesignSize.X, View.Y / DesignSize.Y);
    const FVector2D Origin = (View - DesignSize * S) * 0.5;
    const auto At = [&](FVector2D Point) { return Origin + Point * S; };
    const auto Rect = [&](FVector2D Min, FVector2D Max, FLinearColor Color)
    {
        const FVector2D Position = At(Min);
        const FVector2D Extent = (Max - Min) * S;
        DrawRect(Color, Position.X, Position.Y, Extent.X, Extent.Y);
    };
    const auto Text = [&](const FString& Value, FVector2D Position, float Size, FLinearColor Color, bool bCenter = false)
    {
        UFont* Font = TitleBoldFont ? TitleBoldFont.Get() : GEngine->GetMediumFont();
        const float TextScale = Size * S / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size);
        FVector2D Pixel = At(Position);
        if (bCenter)
        {
            float W, H;
            MeasureUiText(Value, W, H, Font, TextScale);
            Pixel -= FVector2D(W, H) * 0.5;
        }
        DrawUiText(Value, Color, Pixel.X, Pixel.Y, Font, TextScale);
    };
    const auto Frame = [&](const FBox2D& Bounds, FLinearColor Border, FLinearColor Fill)
    {
        Rect(Bounds.Min, Bounds.Max, Border);
        Rect(Bounds.Min + FVector2D(2, 2), Bounds.Max - FVector2D(2, 2), Fill);
    };

    DrawRect(Teal, 0, 0, View.X, View.Y);
    if (ReporterCreationDesk)
    {
        DrawTexture(ReporterCreationDesk, Origin.X, Origin.Y, DesignSize.X * S, DesignSize.Y * S,
            0, 0, 1, 1, FLinearColor::White, BLEND_Opaque);
    }
    else
    {
        Rect({70, 62}, {1530, 856}, FormPaper);
    }

    Text(TEXT("Create Your Reporter"), {800, 112}, 43, Ink, true);
    Rect({115, 151}, {1480, 154}, Teal);
    Frame(FBox2D({112, 180}, {508, 809}), Ink, Pale);
    Rect({126, 194}, {494, 252}, Red);
    Text(TEXT("PRESS"), {310, 223}, 34, Pale, true);
    Frame(FBox2D({150, 268}, {470, 589}), Teal, FLinearColor(0.69f, 0.76f, 0.71f));
    if (const AReporterPawn* Reporter = Cast<AReporterPawn>(GetOwningPawn()))
    {
        const FVector2D Front(0, 1);
        DrawReporter(*Reporter, At({310, 570}), 280 * S, &Front);
    }
    Text(TEXT("Name"), {150, 608}, 20, Ink);
    Frame(MaleButton, bFemaleReporter ? Muted : Teal, bFemaleReporter ? FormPaper : Teal);
    Text(TEXT("Male"), MaleButton.GetCenter(), 17, bFemaleReporter ? Ink : Pale, true);
    Frame(FemaleButton, bFemaleReporter ? Teal : Muted, bFemaleReporter ? Teal : FormPaper);
    Text(TEXT("Female"), FemaleButton.GetCenter(), 17, bFemaleReporter ? Pale : (FemalePoseAtlas ? Ink : Muted), true);

    Text(TEXT("Starting Skills"), {575, 176}, 27, Teal);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(SkillNames); ++Index)
    {
        const int32 Value = ReporterSkills[Index];
        const double Y = 229 + 66 * Index;
        Frame(FBox2D({575, Y}, {1468, Y + 52}), Muted, Pale);
        Text(SkillNames[Index], {594, Y + 11}, 19, Ink);
        Text(FString::Printf(TEXT("Lv. %d"), 1 + (Value - 1) / 20), {915, Y + 11}, 18, Ink);
        Rect({1036, Y + 20}, {1321, Y + 32}, FormPaper);
        Rect({1036, Y + 20}, {1036 + 285.0 * Value / 100.0, Y + 32}, Teal);
        Text(FString::Printf(TEXT("%d/100"), Value), {1345, Y + 12}, 17, Ink);
    }
    Rect(RollButton.Min, RollButton.Max, ReporterRollsLeft ? Teal : Muted);
    Text(TEXT("Roll Skills"), RollButton.GetCenter(), 23, Pale, true);
    Text(FString::Printf(TEXT("%d rolls left"), ReporterRollsLeft), {840, 467}, 18, Ink);

    Rect({575, 535}, {1468, 537}, Teal);
    Text(TEXT("Choose One Talent"), {575, 548}, 25, Teal);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(TalentNames); ++Index)
    {
        const FBox2D Bounds = TalentCard(Index);
        const bool bSelected = int32(ReporterTalent) == Index;
        Frame(Bounds, bSelected ? Teal : Muted, bSelected ? Teal : Pale);
        const FLinearColor Color = bSelected ? Pale : Ink;
        Text(TalentNames[Index], {Bounds.GetCenter().X, Bounds.Min.Y + 29}, 19, Color, true);
        Text(TalentDescriptions[Index], Bounds.Min + FVector2D(14, 55), 13, Color);
    }
    Rect(BeginButton.Min, BeginButton.Max, Red);
    Text(TEXT("Begin Day 1"), BeginButton.GetCenter(), 24, Pale, true);

    if (ReporterCreationControls && ReporterCreationViewport.Equals(View))
    {
        return;
    }
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    if (!Viewport)
    {
        return;
    }
    RemoveReporterCreationControls();
    ReporterCreationViewport = View;
    const auto Anchors = [&](const FBox2D& Box)
    {
        const FVector2D Min = At(Box.Min) / View;
        const FVector2D Max = At(Box.Max) / View;
        return FAnchors(Min.X, Min.Y, Max.X, Max.Y);
    };
    static const FEditableTextBoxStyle NameStyle = []()
    {
        FEditableTextBoxStyle Style = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
        Style.SetBackgroundImageNormal(FSlateColorBrush(FormPaper))
            .SetBackgroundImageHovered(FSlateColorBrush(Pale))
            .SetBackgroundImageFocused(FSlateColorBrush(Pale))
            .SetForegroundColor(FSlateColor(Ink))
            .SetFocusedForegroundColor(FSlateColor(Ink))
            .SetPadding(FMargin(12, 5));
        return Style;
    }();
    auto Controls = SNew(SConstraintCanvas).Visibility(EVisibility::SelfHitTestInvisible);
    Controls->AddSlot().Anchors(Anchors(NameField)).Offset(FMargin(0))
    [
        SNew(SEditableTextBox)
        .Style(&NameStyle)
        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), FMath::Clamp(19 * S, 10.0, 26.0)))
        .Text(FText::FromString(ReporterName))
        .HintText(FText::FromString(TEXT("Alex Reed")))
        .SelectAllTextWhenFocused(true)
        .OnVerifyTextChanged_Lambda([](const FText& Value, FText& Error)
        {
            if (Value.ToString().Len() <= 24)
            {
                return true;
            }
            Error = FText::FromString(TEXT("Use 24 characters or fewer."));
            return false;
        })
        .OnTextChanged_Lambda([this](const FText& Value) { ReporterName = Value.ToString(); })
    ];
    const auto AddButton = [&](const FBox2D& Box, TFunction<void()> Action)
    {
        Controls->AddSlot().Anchors(Anchors(Box)).Offset(FMargin(0))
        [
            SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
            .OnClicked_Lambda([Action = MoveTemp(Action)] { Action(); return FReply::Handled(); })
        ];
    };
    AddButton(MaleButton, [this] { bFemaleReporter = false; });
    if (FemalePoseAtlas)
    {
        AddButton(FemaleButton, [this] { bFemaleReporter = true; });
    }
    AddButton(RollButton, [this]
    {
        if (ReporterRollsLeft > 0)
        {
            --ReporterRollsLeft;
            RollReporterSkills();
        }
    });
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(TalentNames); ++Index)
    {
        AddButton(TalentCard(Index), [this, Index] { ReporterTalent = EReporterTalent(Index); });
    }
    AddButton(BeginButton, [this]
    {
        ReporterName.TrimStartAndEndInline();
        if (ReporterName.IsEmpty())
        {
            ReporterName = TEXT("Alex Reed");
        }
        RemoveReporterCreationControls();
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
        ReporterCreationDesk = nullptr;
        BeginNewsroomPlay();
    });
    ReporterCreationControls = Controls;
    Viewport->AddViewportWidgetContent(Controls, 50);
}
