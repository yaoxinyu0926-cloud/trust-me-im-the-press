#include "ReporterGameMode.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

namespace
{
constexpr double Duration = 60.0;
constexpr int32 Price = 12;
constexpr int32 MealFullness = 30;
constexpr int32 Targets[] = {3, 5};
const TCHAR* FoodNames[] = {TEXT("Mushroom"), TEXT("Lamb"), TEXT("Ginger"), TEXT("Chili")};
constexpr double FoodSize[] = {45, 48, 42, 49};
const FVector2D Center(637, 319);
const FVector2D Wrist(637, 49);
constexpr double RestReach = 104.0;
constexpr double BrothRadius = 273.0;

FVector2D ChopstickDirection(double Degrees)
{
    const double Angle = FMath::DegreesToRadians(Degrees);
    return FVector2D(FMath::Sin(Angle), FMath::Cos(Angle));
}
}

FVector2D AReporterHUD::FHotPotIngredient::GetPosition(double Time) const
{
    const double Angle = Phase + Time * Speed;
    const double Distance = Radius + 10.0 * FMath::Sin(Phase + Time * 0.77);
    return Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
}

void AReporterHUD::LoadHotPotChallenge()
{
    const FString Art = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt");
    HotPotTable = FImageUtils::ImportFileAsTexture2D(Art / TEXT("HotPotTable.jpg"));
    HotPotChopsticks = FImageUtils::ImportFileAsTexture2D(Art / TEXT("HotPotChopsticks.png"));
    for (const TCHAR* Name : FoodNames)
    {
        HotPotFoodArt.Add(FImageUtils::ImportFileAsTexture2D(Art / FString::Printf(TEXT("HotPot%s.png"), Name)));
    }
    if (HotPotFoodArt.Contains(nullptr))
    {
        HotPotChopsticks = nullptr;
    }
}

void AReporterHUD::HandleHotPotClick(bool bUsePointer)
{
    if (HotPotState == EHotPotState::Playing)
    {
        return;
    }
    bool bPrimary = !bUsePointer;
    bool bBack = false;
    if (bUsePointer)
    {
        float X, Y;
        if (GetOwningPlayerController()->GetMousePosition(X, Y))
        {
            const FVector2D Pointer(X, Y);
            bPrimary = HotPotPrimary.bIsValid && HotPotPrimary.IsInsideOrOn(Pointer);
            bBack = HotPotBack.bIsValid && HotPotBack.IsInsideOrOn(Pointer);
        }
    }
    if (bBack || (bPrimary && HotPotState == EHotPotState::Result))
    {
        HotPotState = EHotPotState::Idle;
        return;
    }
    const int32 MealPrice = GetFoodPrice(Price);
    if (!bPrimary || HotPotState != EHotPotState::Order || ReporterCash < MealPrice)
    {
        return;
    }

    ReporterCash -= MealPrice;
    HotPotElapsed = HotPotAimTime = ChopstickAngle = 0;
    ChopstickReach = RestReach;
    HotPotCounts[0] = HotPotCounts[1] = HotPotScore = 0;
    HotPotFeedback.Reset();
    HotPotFeedbackUntil = 0;
    HeldIngredient = INDEX_NONE;
    HotPotIngredients.Reset();
    constexpr int32 Inner[] = {0, 1, 3, 1};
    constexpr int32 Middle[] = {1, 0, 2, 1, 0};
    constexpr int32 Outer[] = {0, 1, 3, 1, 2, 0, 3};
    const auto AddRing = [&](TArrayView<const int32> Kinds, double Radius, double Offset, double Speed)
    {
        for (int32 Index = 0; Index < Kinds.Num(); ++Index)
        {
            FHotPotIngredient& Food = HotPotIngredients.AddDefaulted_GetRef();
            Food.Kind = Kinds[Index];
            Food.Radius = Radius;
            Food.Phase = Offset + Index * 2.0 * UE_PI / Kinds.Num();
            Food.Speed = Speed;
        }
    };
    AddRing(Inner, 91, 0.35, 0.37);
    AddRing(Middle, 159, 1.15, -0.24);
    AddRing(Outer, 225, 0.12, 0.19);
    ChopstickState = EChopstickState::Aiming;
    HotPotState = EHotPotState::Playing;
    StartGymMusic(2);
}

void AReporterHUD::CastChopsticks()
{
    if (HotPotState != EHotPotState::Playing || ChopstickState != EChopstickState::Aiming)
    {
        return;
    }
    const FVector2D Direction = ChopstickDirection(ChopstickAngle);
    const FVector2D ToCenter = Center - Wrist;
    const double Projection = FVector2D::DotProduct(ToCenter, Direction);
    ChopstickLimit = Projection + FMath::Sqrt(FMath::Max(0.0,
        BrothRadius * BrothRadius - ToCenter.SizeSquared() + Projection * Projection));
    ChopstickState = EChopstickState::Extending;
}

void AReporterHUD::UpdateHotPotChallenge(float DeltaSeconds)
{
    if (HotPotState != EHotPotState::Playing)
    {
        return;
    }

    // Bound each collision step so a fast-moving chopstick tip cannot pass through food.
    double Remaining = FMath::Min(double(DeltaSeconds), Duration - HotPotElapsed);
    while (Remaining > UE_SMALL_NUMBER)
    {
        const double Step = FMath::Min(Remaining, 1.0 / 120.0);
        HotPotElapsed += Step;
        Remaining -= Step;

        if (ChopstickState == EChopstickState::Aiming)
        {
            HotPotAimTime += Step;
            ChopstickAngle = 59.0 * FMath::Sin(HotPotAimTime * 1.68);
        }
        else if (ChopstickState == EChopstickState::Extending)
        {
            ChopstickReach = FMath::Min(ChopstickReach + 630.0 * Step, ChopstickLimit);
            const FVector2D Tip = Wrist + ChopstickDirection(ChopstickAngle) * ChopstickReach;
            double Nearest = TNumericLimits<double>::Max();
            int32 Hit = INDEX_NONE;
            for (int32 Index = 0; Index < HotPotIngredients.Num(); ++Index)
            {
                const FHotPotIngredient& Food = HotPotIngredients[Index];
                if (Food.RespawnAt > HotPotElapsed)
                {
                    continue;
                }
                const double Distance = (Food.GetPosition(HotPotElapsed) - Tip).SizeSquared();
                if (Distance < Nearest && Distance <= FMath::Square(FoodSize[Food.Kind] * 0.44))
                {
                    Nearest = Distance;
                    Hit = Index;
                }
            }
            if (Hit != INDEX_NONE)
            {
                HeldIngredient = Hit;
                FHotPotIngredient& Food = HotPotIngredients[Hit];
                Food.RespawnAt = HotPotElapsed + 3.0;
                int32 Points = -10;
                if (Food.Kind < UE_ARRAY_COUNT(Targets))
                {
                    const int32 Count = ++HotPotCounts[Food.Kind];
                    Points = Count <= Targets[Food.Kind] ? 10 : 1;
                }
                HotPotScore += Points;
                HotPotFeedback = FString::Printf(TEXT("%s  %+d"), FoodNames[Food.Kind], Points);
                HotPotFeedbackUntil = HotPotElapsed + 0.9;
                ChopstickState = EChopstickState::Retracting;
            }
            else if (ChopstickReach >= ChopstickLimit)
            {
                HotPotFeedback = TEXT("Miss");
                HotPotFeedbackUntil = HotPotElapsed + 0.7;
                ChopstickState = EChopstickState::Retracting;
            }
        }
        else
        {
            ChopstickReach = FMath::Max(RestReach, ChopstickReach - 760.0 * Step);
            if (ChopstickReach <= RestReach)
            {
                HeldIngredient = INDEX_NONE;
                ChopstickState = EChopstickState::Aiming;
            }
        }
    }
    if (HotPotElapsed >= Duration - UE_SMALL_NUMBER)
    {
        FinishHotPotChallenge();
    }
}

void AReporterHUD::FinishHotPotChallenge()
{
    HotPotState = EHotPotState::Result;
    StopActivityMusic();
    const bool bComplete = HotPotCounts[0] >= Targets[0] && HotPotCounts[1] >= Targets[1];
    HotPotGrade = bComplete && HotPotScore >= 70 ? 'S' :
        (HotPotScore >= 50 ? 'A' : (HotPotScore >= 30 ? 'B' : 'C'));
    HotPotRebate = HotPotGrade == 'S' ? 10 : (HotPotGrade == 'A' ? 7 : (HotPotGrade == 'B' ? 4 : 0));
    HotPotFullnessGain = FMath::Min(MealFullness, 100 - ReporterFullness);
    ReporterFullness += HotPotFullnessGain;
    ReporterCash += HotPotRebate;
}

void AReporterHUD::DrawHotPotChallenge()
{
    HotPotPrimary = FBox2D(ForceInit);
    HotPotBack = FBox2D(ForceInit);
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const double S = FMath::Min(View.X / 1280.0, View.Y / 640.0);
    const FVector2D Size = FVector2D(1280, 640) * S;
    const FVector2D Origin = (View - Size) * 0.5;
    const auto At = [&](FVector2D P) { return Origin + P * S; };
    const FLinearColor Cream(0.98f, 0.89f, 0.73f);
    const FLinearColor Ink(0.12f, 0.055f, 0.038f, 0.96f);
    const FLinearColor Teal(0.09f, 0.32f, 0.25f);
    const auto Box = [&](double X, double Y, double W, double H, FLinearColor Color)
    {
        const FVector2D P = At(FVector2D(X, Y));
        DrawRect(Color, P.X, P.Y, W * S, H * S);
    };
    const auto Text = [&](const FString& Value, double X, double Y, float Scale = 1.0f)
    {
        const FVector2D P = At(FVector2D(X, Y));
        DrawUiText(Value, Cream, P.X, P.Y, GEngine->GetMediumFont(), Scale * S);
    };
    const auto Button = [&](FBox2D& Bounds, const TCHAR* Label, double X, double Y, double W, bool bEnabled = true)
    {
        if (bEnabled)
        {
            Bounds = FBox2D(At(FVector2D(X, Y)), At(FVector2D(X + W, Y + 46)));
        }
        Box(X, Y, W, 46, bEnabled ? Teal : Ink);
        Text(Label, X + 13, Y + 11);
    };
    const auto DrawFood = [&](int32 Kind, FVector2D Position)
    {
        UTexture2D* Art = HotPotFoodArt[Kind];
        const FVector2D ArtSize(Art->GetSizeX(), Art->GetSizeY());
        const FVector2D Extent = ArtSize * (FoodSize[Kind] * S / FMath::Max(ArtSize.X, ArtSize.Y));
        const FVector2D P = At(Position) - Extent * 0.5;
        DrawTexture(Art, P.X, P.Y, Extent.X, Extent.Y, 0, 0, 1, 1, FLinearColor::White, BLEND_Translucent);
    };

    DrawRect(Ink, 0, 0, View.X, View.Y);
    DrawTexture(HotPotTable, Origin.X, Origin.Y, Size.X, Size.Y, 0, 0, 1, 1,
        FLinearColor::White, BLEND_Opaque);
    if (HotPotState == EHotPotState::Order)
    {
        Box(389, 145, 502, 342, Ink);
        Text(TEXT("Hot Pot Challenge"), 415, 166, 1.5f);
        const int32 MealPrice = GetFoodPrice(Price);
        Text(FString::Printf(TEXT("Set meal  $%d    |    Cash  $%d"), MealPrice, ReporterCash), 415, 223);
        Text(TEXT("Catch 3 mushrooms and 5 slices of lamb."), 415, 270);
        Text(TEXT("Ginger and chilies cost points."), 415, 301);
        Text(TEXT("Space to catch    |    60 seconds"), 415, 340);
        Button(HotPotPrimary, ReporterCash >= MealPrice ? TEXT("Order & Play [Enter]") : TEXT("Not enough cash"),
            415, 409, 263, ReporterCash >= MealPrice);
        Button(HotPotBack, TEXT("Back"), 700, 409, 140);
        return;
    }
    if (HotPotState == EHotPotState::Result)
    {
        Box(389, 158, 502, 311, Ink);
        Text(FString::Printf(TEXT("Grade %c   |   %d points"), HotPotGrade, HotPotScore), 415, 184, 1.5f);
        Text(FString::Printf(TEXT("Mushrooms %d / %d    Lamb %d / %d"),
            HotPotCounts[0], Targets[0], HotPotCounts[1], Targets[1]), 415, 245);
        Text(FString::Printf(TEXT("Fullness +%d    |    Meal rebate +$%d"), HotPotFullnessGain, HotPotRebate), 415, 291);
        Text(FString::Printf(TEXT("Cash  $%d"), ReporterCash), 415, 327);
        Button(HotPotPrimary, TEXT("Back to Restaurant [Enter]"), 415, 396, 328);
        return;
    }

    for (const FHotPotIngredient& Food : HotPotIngredients)
    {
        if (Food.RespawnAt <= HotPotElapsed)
        {
            DrawFood(Food.Kind, Food.GetPosition(HotPotElapsed));
        }
    }
    const FVector2D Tip = Wrist + ChopstickDirection(ChopstickAngle) * ChopstickReach;
    const double ToolHeight = 148.0 * S;
    const double ToolWidth = ToolHeight * HotPotChopsticks->GetSizeX() / HotPotChopsticks->GetSizeY();
    const FVector2D Pivot(0.51, 0.98);
    const FVector2D Tool = At(Tip) - FVector2D(ToolWidth, ToolHeight) * Pivot;
    DrawTexture(HotPotChopsticks, Tool.X, Tool.Y, ToolWidth, ToolHeight, 0, 0, 1, 1,
        FLinearColor::White, BLEND_Translucent, 1, false, -ChopstickAngle, Pivot);
    if (HotPotIngredients.IsValidIndex(HeldIngredient))
    {
        DrawFood(HotPotIngredients[HeldIngredient].Kind, Tip);
    }

    Box(19, 115, 222, 248, Ink);
    Text(TEXT("Order"), 35, 132, 1.25f);
    DrawFood(0, FVector2D(59, 203));
    DrawFood(1, FVector2D(59, 257));
    Text(FString::Printf(TEXT("Mushrooms %d/%d"), HotPotCounts[0], Targets[0]), 91, 192, 0.9f);
    Text(FString::Printf(TEXT("Lamb %d/%d"), HotPotCounts[1], Targets[1]), 91, 246, 0.9f);
    Text(TEXT("Avoid ginger & chili"), 35, 315, 0.9f);
    Box(1039, 115, 222, 174, Ink);
    Text(FString::Printf(TEXT("Time   %02d s"), FMath::Max(0, FMath::CeilToInt(Duration - HotPotElapsed))), 1056, 132, 1.2f);
    Text(FString::Printf(TEXT("Score  %d"), HotPotScore), 1056, 179);
    Text(HotPotElapsed < HotPotFeedbackUntil ? HotPotFeedback : TEXT("Space to catch"), 1056, 229);
}
