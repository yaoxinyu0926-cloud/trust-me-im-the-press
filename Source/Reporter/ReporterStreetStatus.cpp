#include "ReporterGameMode.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "TextureResource.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"

namespace
{
constexpr double StatusWidth = 196;
constexpr double StatusRightMargin = 14;
constexpr double StatusTop = 14;
constexpr double ShortcutGap = 6;
constexpr double ShortcutWidth = (StatusWidth - ShortcutGap) / 2;
constexpr double ShortcutHeight = 30;
const FLinearColor HudInk(0.10f, 0.075f, 0.049f);
const FLinearColor HudPaper(0.98f, 0.90f, 0.75f);
const FLinearColor HudSelected(0.71f, 0.83f, 0.76f);

double StatusHeight(bool bIndoors)
{
    return bIndoors ? 68 : 102;
}
}

void AReporterHUD::DrawHudRoundedRect(double X, double Y, double W, double H, double Radius, FLinearColor Color)
{
    const double S = GetHudScale();
    if (W <= 0 || H <= 0)
    {
        return;
    }
    Radius = FMath::Min(Radius, FMath::Min(W, H) * 0.5);
    const FVector2D Corners[] = {
        {X + W - Radius, Y + H - Radius}, {X + Radius, Y + H - Radius},
        {X + Radius, Y + Radius}, {X + W - Radius, Y + Radius}
    };
    constexpr int32 Steps = 6;
    TArray<FVector2D, TInlineAllocator<4 * (Steps + 1)>> Rim;
    for (int32 Corner = 0; Corner < 4; ++Corner)
    {
        for (int32 Step = 0; Step <= Steps; ++Step)
        {
            const double Angle = UE_PI * 0.5 * (Corner + double(Step) / Steps);
            Rim.Add((Corners[Corner] + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius) * S);
        }
    }
    TArray<FCanvasUVTri> Triangles;
    Triangles.Reserve(Rim.Num());
    for (int32 Index = 0; Index < Rim.Num(); ++Index)
    {
        FCanvasUVTri& Tri = Triangles.AddDefaulted_GetRef();
        Tri.V0_Pos = FVector2D(X + W * 0.5, Y + H * 0.5) * S;
        Tri.V1_Pos = Rim[Index];
        Tri.V2_Pos = Rim[(Index + 1) % Rim.Num()];
        Tri.V0_UV = Tri.V1_UV = Tri.V2_UV = FVector2D(0.5, 0.5);
        Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Color;
    }
    FCanvasTriangleItem Item(Triangles, Canvas->DefaultTexture->GetResource());
    Canvas->DrawItem(Item);
}

FBox2D AReporterHUD::DrawHudCard(double X, double Y, double W, double H, FLinearColor Fill)
{
    DrawHudRoundedRect(X + 2, Y + 3, W, H, 10, FLinearColor(0.16f, 0.10f, 0.06f));
    DrawHudRoundedRect(X, Y, W, H, 10, HudInk);
    DrawHudRoundedRect(X + 2, Y + 2, W - 4, H - 4, 8, FLinearColor(0.77f, 0.58f, 0.39f));
    DrawHudRoundedRect(X + 3, Y + 3, W - 6, H - 6, 7, Fill);
    const double S = GetHudScale();
    return FBox2D(FVector2D(X, Y) * S, FVector2D(X + W, Y + H) * S);
}

FVector2D AReporterHUD::GetHudShortcutOrigin() const
{
    return FVector2D(Canvas->ClipX / GetHudScale() - StatusRightMargin - StatusWidth,
        StatusTop + StatusHeight(bInLobby) + 7);
}

FBox2D AReporterHUD::DrawHudShortcut(FVector2D Pos, bool bBackpack, bool bEnabled)
{
    const double S = GetHudScale();
    const FVector2D Size(ShortcutWidth, ShortcutHeight);
    const FBox2D HitBox(Pos * S, (Pos + Size) * S);
    float MouseX = 0, MouseY = 0;
    const APlayerController* Player = GetOwningPlayerController();
    const bool bHover = bEnabled && Player && Player->GetMousePosition(MouseX, MouseY) &&
        HitBox.IsInsideOrOn(FVector2D(MouseX, MouseY));
    const FLinearColor Fill = !bEnabled ? FLinearColor(0.85f, 0.78f, 0.66f)
        : bHover || (bBackpack && bBackpackOpen) ? HudSelected : HudPaper;
    const FLinearColor Accent = bBackpack ? FLinearColor(0.55f, 0.32f, 0.19f) : FLinearColor(0.18f, 0.43f, 0.39f);
    DrawHudCard(Pos.X, Pos.Y, Size.X, Size.Y, Fill);

    const FVector2D Icon = Pos + FVector2D(14, 15);
    const auto RoundIcon = [&](double X, double Y, double W, double H, double Radius, FLinearColor Color)
    {
        DrawHudRoundedRect(Icon.X + X, Icon.Y + Y, W, H, Radius, Color);
    };
    if (bBackpack)
    {
        RoundIcon(-3, -8, 6, 6, 3, HudInk);
        RoundIcon(-1.5, -6.5, 3, 3, 1.5, Fill);
        RoundIcon(-7, -5, 14, 12, 3, HudInk);
        RoundIcon(-5.5, -3.5, 11, 9, 2, Accent);
        RoundIcon(-4.5, 0, 9, 1, 0.5, HudInk);
        RoundIcon(-1.2, 0, 2.4, 3.8, 0.7, HudPaper);
    }
    else
    {
        for (int32 Index = 0; Index < 8; ++Index)
        {
            const double Angle = Index * UE_TWO_PI / 8.0;
            const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
            const FVector2D A = (Icon + Direction * 4) * S;
            const FVector2D B = (Icon + Direction * 8) * S;
            DrawLine(A.X, A.Y, B.X, B.Y, HudInk, 2.3 * S);
        }
        RoundIcon(-6, -6, 12, 12, 6, HudInk);
        RoundIcon(-4.5, -4.5, 9, 9, 4.5, Accent);
        RoundIcon(-2, -2, 4, 4, 2, Fill);
    }

    UFont* Font = GEngine->GetMediumFont();
    const double KeyWidth = bBackpack ? 15 : 23;
    const double KeyX = Pos.X + ShortcutWidth - KeyWidth - 6;
    DrawHudRoundedRect(KeyX, Pos.Y + 7, KeyWidth, 16, 4, FLinearColor(0.57f, 0.46f, 0.34f));
    DrawHudRoundedRect(KeyX + 1, Pos.Y + 8, KeyWidth - 2, 14, 3, FLinearColor(0.96f, 0.88f, 0.73f));

    const auto FitLabel = [&](const TCHAR* Label, double X, double Width, float Scale, bool bCenter)
    {
        Scale *= S;
        float W, H;
        MeasureUiText(Label, W, H, Font, Scale);
        Scale *= FMath::Min(1.0, Width * S / FMath::Max(1.0f, W));
        MeasureUiText(Label, W, H, Font, Scale);
        DrawUiText(Label, HudInk, X * S + (bCenter ? (Width * S - W) / 2 : 0),
            (Pos.Y + ShortcutHeight / 2) * S - H / 2, Font, Scale);
    };
    const double LabelX = Pos.X + 24;
    FitLabel(bBackpack ? TEXT("Backpack") : TEXT("Settings"), LabelX, KeyX - LabelX - 3, 0.92f, false);
    FitLabel(bBackpack ? TEXT("B") : TEXT("Esc"), KeyX + 1, KeyWidth - 2, 0.72f, true);
    return bEnabled ? HitBox : FBox2D(ForceInit);
}

double AReporterHUD::GetHudScale() const
{
    return FMath::Min(Canvas->ClipY / 720.0, Canvas->ClipX / 650.0);
}

void AReporterHUD::DrawStreetStatus()
{
    const double S = GetHudScale();
    const double Right = Canvas->ClipX / S;
    const FLinearColor& Ink = HudInk;
    const FLinearColor& PanelFill = HudPaper;
    const FLinearColor Teal(0.12f, 0.46f, 0.43f);
    const FLinearColor Ochre(0.75f, 0.48f, 0.12f);
    const auto Rect = [&](double X, double Y, double W, double H, FLinearColor Color)
    {
        DrawRect(Color, X * S, Y * S, W * S, H * S);
    };
    constexpr float HudTextScale = 1.16f;
    const auto Text = [&](const FString& Value, double X, double Y, float Size = 1.0f)
    {
        DrawUiText(Value, Ink, X * S, Y * S, GEngine->GetLargeFont(), HudTextScale * Size * S);
    };
    const auto Rounded = [&](double X, double Y, double W, double H, double Radius, FLinearColor Color)
    {
        DrawHudRoundedRect(X, Y, W, H, Radius, Color);
    };
    const auto Card = [&](double X, double Y, double W, double H)
    {
        return DrawHudCard(X, Y, W, H, PanelFill);
    };
    const auto Bar = [&](double Y, const TCHAR* Label, double Fill, FLinearColor Color)
    {
        Text(Label, 84, Y - 5);
        Rounded(181, Y, 122, 15, 7.5, Ink);
        Rounded(183, Y + 2, 118, 11, 5.5, FLinearColor(0.90f, 0.80f, 0.64f));
        Rounded(183, Y + 2, 118 * Fill, 11, 5.5, Color);
    };

    Card(44, 18, 270, 66);
    ReporterPortrait = FBox2D(FVector2D(12, 12) * S, FVector2D(80, 80) * S);
    Rounded(12, 12, 68, 68, 34, Ink);
    Rounded(14, 14, 64, 64, 32, PanelFill);
    Rounded(17, 17, 58, 58, 29, Ink);
    Rounded(18, 18, 56, 56, 28, FLinearColor(0.80f, 0.65f, 0.45f));
    if (const UTexture2D* Atlas = GetActivePoseAtlas())
    {
        const FVector2D Center(46, 46);
        const FVector2D TextureSize(Atlas->GetSizeX(), Atlas->GetSizeY());
        const FVector2D SourceCenter = Atlas == FemalePoseAtlas ? FVector2D(232, 91) : FVector2D(231, 90);
        TArray<FCanvasUVTri> Triangles;
        constexpr int32 Segments = 36;
        for (int32 Index = 0; Index < Segments; ++Index)
        {
            const double A = UE_TWO_PI * Index / Segments;
            const double B = UE_TWO_PI * (Index + 1) / Segments;
            const FVector2D P(FMath::Cos(A), FMath::Sin(A));
            const FVector2D Q(FMath::Cos(B), FMath::Sin(B));
            FCanvasUVTri& Tri = Triangles.AddDefaulted_GetRef();
            Tri.V0_Pos = Center * S;
            Tri.V1_Pos = (Center + P * 27.5) * S;
            Tri.V2_Pos = (Center + Q * 27.5) * S;
            Tri.V0_UV = SourceCenter / TextureSize;
            Tri.V1_UV = (SourceCenter + P * 74) / TextureSize;
            Tri.V2_UV = (SourceCenter + Q * 74) / TextureSize;
            Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = FLinearColor::White;
        }
        FCanvasTriangleItem Portrait(Triangles, Atlas->GetResource());
        Portrait.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Portrait);
    }
    Bar(35, TEXT("Stamina"), ReporterStamina / 100.0, Teal);
    Bar(60, TEXT("Fullness"), ReporterFullness / 100.0, Ochre);

    const int64 Minutes = FMath::FloorToInt64(OutdoorGameMinutes);
    const int64 Day = Minutes / 1440 + 1;
    const int32 Hour = (Minutes % 1440) / 60;
    const int32 Minute = Minutes % 60;
    constexpr double CardWidth = StatusWidth;
    constexpr double CardTop = StatusTop;
    const double X = Right - CardWidth - StatusRightMargin;
    const double CenterX = X + CardWidth * 0.5;
    Card(X, CardTop, CardWidth, StatusHeight(bInLobby));
    const auto Measure = [&](const FString& Value, float Size)
    {
        float Width, Height;
        MeasureUiText(Value, Width, Height, GEngine->GetLargeFont(), HudTextScale * Size * S);
        return FVector2D(Width, Height) / S;
    };
    const auto Centered = [&](const FString& Value, double CenterY, float Size)
    {
        const FVector2D Extent = Measure(Value, Size);
        Text(Value, CenterX - Extent.X * 0.5, CenterY - Extent.Y * 0.5, Size);
    };
    const FString Clock = FString::Printf(TEXT("DAY %lld  |  %02d:%02d"), Day, Hour, Minute);
    constexpr float ClockSize = 1.26f;
    const double ClockWidth = Measure(Clock, ClockSize).X;
    Centered(Clock, CardTop + 17, ClockSize * FMath::Min(1.0, (CardWidth - 22) / FMath::Max(1.0, ClockWidth)));
    Rect(X + 11, CardTop + 33, CardWidth - 22, 1, Ink);

    if (!bInLobby)
    {
        constexpr float WeatherSize = 1.32f;
        constexpr double CloudWidth = 29;
        constexpr double Gap = 8;
        const FString Weather(IsRainyDay() ? TEXT("Rainy") : TEXT("Cloudy"));
        const FVector2D WeatherExtent = Measure(Weather, WeatherSize);
        const double CloudX = CenterX - (CloudWidth + Gap + WeatherExtent.X) * 0.5;
        const double WeatherY = CardTop + 50;
        Text(Weather, CloudX + CloudWidth + Gap, WeatherY - WeatherExtent.Y * 0.5, WeatherSize);
        Rounded(CloudX, WeatherY - 2, CloudWidth, 12, 6, Ink);
        Rounded(CloudX + 6, WeatherY - 9, 15, 15, 7.5, Ink);
        const FLinearColor CloudFill(0.52f, 0.68f, 0.71f);
        Rounded(CloudX + 2, WeatherY, 25, 8, 4, CloudFill);
        Rounded(CloudX + 8, WeatherY - 7, 11, 13, 5.5, CloudFill);
        Rect(X + 11, CardTop + 67, CardWidth - 22, 1, Ink);
    }
    Centered(FString::Printf(TEXT("$%d"), ReporterCash), CardTop + (bInLobby ? 50 : 85), 1.43f);

    const FVector2D Shortcuts = GetHudShortcutOrigin();
    const FBox2D Bag = DrawHudShortcut(Shortcuts, true, !bDecorating);
    if (Bag.bIsValid)
    {
        FindActions.Emplace(Bag, EFindAction::Bag);
    }
    SettingsShortcut = DrawHudShortcut(Shortcuts + FVector2D(ShortcutWidth + ShortcutGap, 0), false, true);
    DrawEndingWarnings();

    if (bReporterPanelOpen)
    {
        ReporterPanel = Card(14, 99, 300, 234);
        Text(ReporterName, 27, 109, 1.05f);
        ReporterTabs[0] = Card(26, 142, 102, 28);
        ReporterTabs[1] = Card(138, 142, 102, 28);
        Rect(bShowReporterSkills ? 141 : 29, 166, 96, 2, Teal);
        Text(TEXT("Status"), 40, 147);
        Text(TEXT("Skills"), 151, 147);
        if (bShowReporterSkills)
        {
            const TCHAR* Skills[] = {TEXT("Interviewing"), TEXT("Investigation"), TEXT("Editing")};
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(Skills); ++Index)
            {
                const int32 Value = ReporterSkills[Index];
                Text(FString::Printf(TEXT("%s   Lv. %d  (%d/100)"), Skills[Index], 1 + (Value - 1) / 20, Value),
                    30, 185 + Index * 27, 0.83f);
            }
        }
        else
        {
            Text(FString::Printf(TEXT("Health      %d / %d"), ReporterHealth, MaxHealth), 30, 185);
            Text(FString::Printf(TEXT("Stamina     %d / 100"), ReporterStamina), 30, 212);
            Text(TEXT("Mood        80 / 100"), 30, 239);
            Text(FString::Printf(TEXT("Fullness    %d / 100"), ReporterFullness), 30, 266);
            Text(FString::Printf(TEXT("Reputation  %d / 100"), GetReporterReputation()), 30, 293);
        }
    }
}

bool AReporterHUD::ClickStreetStatus(FVector2D Pointer)
{
    if (ReporterPortrait.bIsValid && ReporterPortrait.IsInsideOrOn(Pointer))
    {
        bReporterPanelOpen = !bReporterPanelOpen;
        return true;
    }
    if (!bReporterPanelOpen)
    {
        return false;
    }
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ReporterTabs); ++Index)
    {
        if (ReporterTabs[Index].bIsValid && ReporterTabs[Index].IsInsideOrOn(Pointer))
        {
            bShowReporterSkills = Index == 1;
            return true;
        }
    }
    if (!ReporterPanel.IsInsideOrOn(Pointer))
    {
        bReporterPanelOpen = false;
    }
    return true;
}
