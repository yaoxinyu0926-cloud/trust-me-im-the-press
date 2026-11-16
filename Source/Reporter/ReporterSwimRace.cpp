#include "ReporterGameMode.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

namespace
{
constexpr int32 PlayerLane = 2;
constexpr double ReadyTime = 3.0;
constexpr double OpponentSprintStart = 16.0;

bool OpponentStrokes(int32 Lane, double Seconds)
{
    if (Lane == 0)
    {
        return Seconds < 3.4 || FMath::Fmod(Seconds - 3.4, 4.0) >= 1.25;
    }
    if (Lane == 1)
    {
        return FMath::Fmod(Seconds, 3.8) < 2.65;
    }
    return Seconds >= OpponentSprintStart || FMath::Fmod(Seconds, 4.0) < 2.35;
}
}

void AReporterHUD::StartSwimRace()
{
    for (FSwimmer& Swimmer : Swimmers)
    {
        Swimmer = FSwimmer{};
    }
    SwimReadyTime = ReadyTime;
    SwimElapsed = 0;
    SwimBreath = 1;
    bForcedBreath = false;
    SwimState = ESwimState::Ready;
    StartGymMusic();
}

void AReporterHUD::StopSwimRace()
{
    if (SwimState != ESwimState::Idle)
    {
        StopActivityMusic();
        SwimState = ESwimState::Idle;
    }
}

void AReporterHUD::UpdateSwimRace(float DeltaSeconds)
{
    if (SwimState == ESwimState::Ready)
    {
        SwimReadyTime -= DeltaSeconds;
        if (SwimReadyTime <= 0)
        {
            SwimState = ESwimState::Racing;
        }
        return;
    }
    if (SwimState != ESwimState::Racing)
    {
        return;
    }

    const double Step = FMath::Min(DeltaSeconds, 0.05f);
    const APlayerController* Player = GetOwningPlayerController();
    const bool bHoldStroke = Player && Player->IsInputKeyDown(EKeys::SpaceBar);
    FSwimmer& You = Swimmers[PlayerLane];
    You.bStroke = bHoldStroke && !bForcedBreath;
    SwimBreath = FMath::Clamp(SwimBreath + Step * (You.bStroke ? -0.29 : 0.57), 0.0, 1.0);
    if (SwimBreath <= 0)
    {
        bForcedBreath = true;
    }
    else if (SwimBreath >= 0.60)
    {
        bForcedBreath = false;
    }

    for (int32 Lane = 0; Lane < UE_ARRAY_COUNT(Swimmers); ++Lane)
    {
        FSwimmer& Swimmer = Swimmers[Lane];
        if (Swimmer.Finish >= 0)
        {
            continue;
        }

        double StrokeSpeed = 0.061;
        if (Lane != PlayerLane)
        {
            Swimmer.bStroke = OpponentStrokes(Lane, SwimElapsed);
            StrokeSpeed = Lane == 0 && SwimElapsed < 3.4 ? 0.068 :
                (Lane == 3 ? (SwimElapsed >= OpponentSprintStart ? 0.072 : 0.055) : 0.058);
        }
        const double GlideSpeed = Lane == PlayerLane && bForcedBreath ? 0.012 : 0.024;
        const double TargetSpeed = Swimmer.bStroke ? StrokeSpeed : GlideSpeed;
        const double Response = Lane == PlayerLane && bForcedBreath ? 7.0 : 3.0;
        Swimmer.Speed = FMath::Lerp(Swimmer.Speed, TargetSpeed, 1.0 - FMath::Exp(-Response * Step));
        const double Distance = Swimmer.Speed * Step;
        if (Distance > 0 && Swimmer.Distance + Distance >= 1.0)
        {
            Swimmer.Finish = SwimElapsed + Step * (1.0 - Swimmer.Distance) / Distance;
            Swimmer.Distance = 1.0;
            Swimmer.bStroke = false;
        }
        else
        {
            Swimmer.Distance += Distance;
        }
    }
    SwimElapsed += Step;
    if (You.Finish >= 0)
    {
        SwimState = ESwimState::Result;
        StopActivityMusic();
    }
}

void AReporterHUD::HandleSwimClick(bool bUsePointer)
{
    if (SwimState != ESwimState::Result)
    {
        return;
    }
    float X, Y;
    if (!bUsePointer)
    {
        StartSwimRace();
    }
    else if (GetOwningPlayerController()->GetMousePosition(X, Y))
    {
        const FVector2D Pointer(X, Y);
        if (SwimRetry.IsInsideOrOn(Pointer))
        {
            StartSwimRace();
        }
        else if (SwimExit.IsInsideOrOn(Pointer))
        {
            StopSwimRace();
        }
    }
}

void AReporterHUD::DrawSwimRace(FVector2D TopLeft, FVector2D Size)
{
    const FLinearColor Caps[] = {
        FLinearColor(0.94f, 0.34f, 0.17f), FLinearColor(0.64f, 0.45f, 0.90f),
        FLinearColor(1.0f, 0.84f, 0.28f), FLinearColor(0.35f, 0.84f, 0.52f)
    };
    constexpr double LaneY[] = {0.593, 0.65, 0.712, 0.783};
    const FLinearColor Ink(0.07f, 0.14f, 0.17f, 0.97f);
    const FLinearColor Cream(0.98f, 0.93f, 0.79f);
    const double S = Size.Y / 720.0;
    const auto At = [&](double X, double Y) { return TopLeft + Size * FVector2D(X, Y); };
    for (int32 Lane = 0; Lane < UE_ARRAY_COUNT(Swimmers); ++Lane)
    {
        const FSwimmer& Swimmer = Swimmers[Lane];
        const double Depth = (LaneY[Lane] - 0.593) / 0.19;
        const FVector2D P = At(FMath::Lerp(0.215 - Depth * 0.075, 0.785 + Depth * 0.065,
            Swimmer.Distance), LaneY[Lane]);
        const double Phase = SwimElapsed * 17.0 + Lane;
        const bool bMoving = SwimState == ESwimState::Racing && Swimmer.Finish < 0;
        const double HeadY = P.Y - (Swimmer.bStroke ? 5 : 9) * S;
        const double Arm = bMoving && Swimmer.bStroke ? FMath::Sin(Phase) : 0;
        const FLinearColor Skin(0.93f, 0.65f, 0.44f);
        DrawLine(P.X - 21 * S, P.Y + 3 * S, P.X - 7 * S, P.Y + 3 * S, Cream, 2 * S);
        DrawLine(P.X - 12 * S, P.Y, P.X + 1 * S, P.Y, Caps[Lane], 5 * S);
        DrawLine(P.X - 5 * S, P.Y, P.X + (4 + Arm * 8) * S, P.Y - (3 + Arm * 3) * S, Skin, 3 * S);
        DrawRect(Ink, P.X - 3 * S, HeadY - S, 12 * S, 10 * S);
        DrawRect(Caps[Lane], P.X - 2 * S, HeadY, 9 * S, 5 * S);
        DrawRect(Skin, P.X, HeadY + 5 * S, 8 * S, 3 * S);
        if (Lane == PlayerLane)
        {
            DrawUiText(TEXT("YOU"), Cream, P.X - 12 * S, HeadY - 20 * S, GEngine->GetSmallFont(), S);
        }
    }

    const FVector2D Origin = At(0.5, 0.12) - FVector2D(200 * S, 0);
    const auto Rect = [&](double X, double Y, double W, double H, FLinearColor Color)
    {
        DrawRect(Color, Origin.X + X * S, Origin.Y + Y * S, W * S, H * S);
    };
    const auto Text = [&](const FString& Label, double X, double Y, FLinearColor Color = FLinearColor::White)
    {
        DrawUiText(Label, Color, Origin.X + X * S, Origin.Y + Y * S, GEngine->GetMediumFont(), S);
    };
    Rect(0, 0, 400, 116, Ink);
    if (SwimState == ESwimState::Result)
    {
        const double Finish = Swimmers[PlayerLane].Finish;
        int32 Place = 1;
        for (int32 Lane = 0; Lane < UE_ARRAY_COUNT(Swimmers); ++Lane)
        {
            if (Lane != PlayerLane && Swimmers[Lane].Finish >= 0 && Swimmers[Lane].Finish < Finish)
            {
                ++Place;
            }
        }
        Text(FString::Printf(TEXT("Place %d / 4     Time %.2fs"), Place, Finish), 15, 18, Cream);
        SwimRetry = FBox2D(Origin + FVector2D(15, 63) * S, Origin + FVector2D(182, 100) * S);
        SwimExit = FBox2D(Origin + FVector2D(192, 63) * S, Origin + FVector2D(355, 100) * S);
        Rect(15, 63, 167, 37, FLinearColor(0.15f, 0.43f, 0.39f));
        Rect(192, 63, 163, 37, FLinearColor(0.27f, 0.31f, 0.34f));
        Text(TEXT("Retry [Enter]"), 25, 72);
        Text(TEXT("Back [Q]"), 202, 72);
        return;
    }
    Text(SwimState == ESwimState::Ready
        ? FString::Printf(TEXT("Get ready  %d"), FMath::CeilToInt(SwimReadyTime))
        : FString::Printf(TEXT("Swim Race   %.1fs"), SwimElapsed), 15, 12, Cream);
    Text(TEXT("Quit [Q]"), 306, 12);
    Text(TEXT("Breath"), 15, 46);
    Rect(91, 48, 281, 15, FLinearColor(0.24f, 0.28f, 0.29f));
    Rect(91, 48, 281 * SwimBreath, 15, bForcedBreath ? Caps[0] : Caps[3]);
    Text(bForcedBreath ? TEXT("Catching your breath...") :
        TEXT("Hold Space to swim   |   Release to breathe"), 15, 82);
}
