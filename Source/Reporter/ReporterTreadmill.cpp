#include "ReporterGameMode.h"

#include "Components/AudioComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
constexpr double Beat = 60.0 / 108.0;
constexpr int32 ClipLength = 32;
constexpr double FirstBeat = 0.02;
constexpr double HitWindow = 0.17;
constexpr int32 ReadyBeats = 4;

const TArray<double>& RunChart()
{
    static const TArray<double> Notes = []
    {
        TArray<double> Times;
        for (int32 Index = ReadyBeats; FirstBeat + Index * Beat + HitWindow < ClipLength; ++Index)
        {
            // Start on every other beat, then add pickups before the full-speed section.
            if ((Index < 24 && Index % 2 != 0) || (Index >= 24 && Index < 36 && Index % 4 == 2))
            {
                continue;
            }
            Times.Add(FirstBeat + Index * Beat);
        }
        return Times;
    }();
    return Notes;
}
}

bool AReporterHUD::StartGymMusic(int32 Repeats)
{
    const FString Path = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt/GymGroove.pcm");
    TArray<uint8> Samples;
    if (!FFileHelper::LoadFileToArray(Samples, *Path) || Samples.Num() != 48000 * 2 * sizeof(int16) * ClipLength)
    {
        UE_LOG(LogTemp, Warning, TEXT("Gym music could not be loaded: %s"), *Path);
        return false;
    }
    USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
    Wave->SetSampleRate(48000);
    Wave->NumChannels = 2;
    Wave->Duration = ClipLength * Repeats;
    for (int32 Index = 0; Index < Repeats; ++Index)
    {
        Wave->QueueAudio(Samples.GetData(), Samples.Num());
    }
    return PlayActivityMusic(Wave);
}

void AReporterHUD::StartTreadmillRun()
{
    StopTreadmillRun();
    if (!StartGymMusic())
    {
        return;
    }
    RunStarted = GetWorld()->GetTimeSeconds();
    RunNote = RunHits = RunCombo = RunBest = 0;
    RunFeedbackUntil = 0;
    RunState = ERunState::Running;
}

bool AReporterHUD::PlayActivityMusic(USoundBase* Track)
{
    UAudioComponent* Next = UGameplayStatics::CreateSound2D(this, Track);
    if (!Next)
    {
        return false;
    }
    if (IsValid(ActivityMusicPlayer))
    {
        ActivityMusicPlayer->Stop();
    }
    if (IsValid(BackgroundMusicPlayer))
    {
        BackgroundMusicPlayer->SetPaused(true);
    }
    ActivityMusicPlayer = Next;
    ApplyAudioSettings();
    ActivityMusicPlayer->Play();
    return true;
}

void AReporterHUD::StopActivityMusic()
{
    if (IsValid(ActivityMusicPlayer))
    {
        ActivityMusicPlayer->Stop();
    }
    ActivityMusicPlayer = nullptr;
    if (IsValid(BackgroundMusicPlayer))
    {
        BackgroundMusicPlayer->SetPaused(false);
    }
}

void AReporterHUD::StopTreadmillRun()
{
    StopActivityMusic();
    RunState = ERunState::Idle;
}

void AReporterHUD::UpdateTreadmillRun()
{
    if (RunState != ERunState::Running)
    {
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    const double Elapsed = Now - RunStarted;
    const TArray<double>& Notes = RunChart();
    while (Notes.IsValidIndex(RunNote) && Elapsed > Notes[RunNote] + HitWindow)
    {
        ++RunNote;
        RunCombo = 0;
        RunFeedback = TEXT("Miss");
        RunFeedbackUntil = Now + 0.22;
    }
    if (Elapsed >= ClipLength)
    {
        StopTreadmillRun();
        RunState = ERunState::Result;
    }
}

void AReporterHUD::RunStep(FKey Key)
{
    if (TitlePage == ETitlePage::Comic && OpeningComicPage)
    {
        if (Key == EKeys::SpaceBar)
        {
            AdvanceOpeningComic();
        }
        return;
    }
    if (HotPotState != EHotPotState::Idle)
    {
        if (Key == EKeys::SpaceBar)
        {
            CastChopsticks();
        }
        return;
    }
    if (SwimState != ESwimState::Idle)
    {
        if (Key == EKeys::Q)
        {
            StopSwimRace();
        }
        return;
    }
    if (Key == EKeys::Q && RunState != ERunState::Idle)
    {
        StopTreadmillRun();
        return;
    }
    UpdateTreadmillRun();
    if (RunState != ERunState::Running || (Key != EKeys::A && Key != EKeys::D))
    {
        return;
    }
    const TArray<double>& Notes = RunChart();
    const double Now = GetWorld()->GetTimeSeconds();
    if (!Notes.IsValidIndex(RunNote) || Now - RunStarted < Notes[0] - HitWindow)
    {
        return;
    }
    const double Error = FMath::Abs(Now - RunStarted - Notes[RunNote]);
    if (Error <= HitWindow && (Key == EKeys::A) == (RunNote % 2 == 0))
    {
        ++RunNote;
        ++RunHits;
        RunBest = FMath::Max(RunBest, ++RunCombo);
        RunFeedback = Error <= 0.065 ? TEXT("Perfect") : TEXT("Good");
    }
    else
    {
        RunCombo = 0;
        RunFeedback = TEXT("Off beat");
    }
    RunFeedbackUntil = Now + 0.22;
}

void AReporterHUD::HandleTreadmillClick(bool bUsePointer)
{
    if (RunState != ERunState::Result)
    {
        return;
    }
    if (!bUsePointer)
    {
        StartTreadmillRun();
        return;
    }
    double X, Y;
    if (GetOwningPlayerController()->GetMousePosition(X, Y))
    {
        if (RunRetry.IsInsideOrOn(FVector2D(X, Y))) { StartTreadmillRun(); }
        else if (RunExit.IsInsideOrOn(FVector2D(X, Y))) { StopTreadmillRun(); }
    }
}

void AReporterHUD::DrawTreadmillRun(const AReporterPawn& Reporter, double RoomHeight)
{
    const double Now = GetWorld()->GetTimeSeconds();
    const double Elapsed = Now - RunStarted;
    bIndoorWalking = RunState == ERunState::Running && Elapsed >= RunChart()[0];
    const FVector2D Right(1, 0);
    DrawReporter(Reporter, RunFoot, RoomHeight * 0.25, &Right);

    const double S = FMath::Min(Canvas->ClipX / 720.0, Canvas->ClipY / 540.0);
    const FVector2D Origin(Canvas->ClipX * 0.5 - 245 * S, 20 * S);
    const FLinearColor Ink(0.07f, 0.13f, 0.14f);
    const FLinearColor Cream(0.97f, 0.89f, 0.73f);
    const FLinearColor Teal(0.15f, 0.54f, 0.50f);
    const FLinearColor Amber(0.92f, 0.57f, 0.24f);
    const auto Rect = [&](double X, double Y, double W, double H, FLinearColor Color)
    {
        DrawRect(Color, Origin.X + X * S, Origin.Y + Y * S, W * S, H * S);
    };
    const auto Text = [&](const FString& Value, double X, double Y, FLinearColor Color, float Scale = 1.0f)
    {
        DrawUiText(Value, Color, Origin.X + X * S, Origin.Y + Y * S, GEngine->GetMediumFont(), S * Scale);
    };
    Rect(0, 0, 490, 171, Ink);
    Rect(2, 2, 486, 167, Cream);
    Text(TEXT("Treadmill"), 15, 10, Ink, 1.25f);
    if (RunState == ERunState::Result)
    {
        const int32 Accuracy = FMath::RoundToInt(100.0 * RunHits / RunChart().Num());
        Text(FString::Printf(TEXT("Accuracy: %d%%     Best combo: %d"), Accuracy, RunBest), 15, 52, Ink, 1.14f);
        RunRetry = FBox2D(Origin + FVector2D(15, 103) * S, Origin + FVector2D(185, 143) * S);
        RunExit = FBox2D(Origin + FVector2D(198, 103) * S, Origin + FVector2D(322, 143) * S);
        Rect(15, 103, 170, 40, Teal);
        Rect(198, 103, 124, 40, Ink);
        Text(TEXT("Retry [Enter]"), 24, 115, FLinearColor::White);
        Text(TEXT("Back [Q]"), 210, 115, FLinearColor::White);
        return;
    }
    Text(FString::Printf(TEXT("Combo %d   |   %ds"), RunCombo, FMath::CeilToInt(ClipLength - Elapsed)),
        317, 12, Ink);
    for (int32 Lane = 0; Lane < 2; ++Lane)
    {
        const double Y = 46 + Lane * 37;
        Text(Lane == 0 ? TEXT("A") : TEXT("D"), 15, Y + 4, Ink, 1.1f);
        Rect(40, Y, 435, 30, Ink);
        Rect(69, Y + 1, 3, 28, Cream);
    }
    const double ApproachTime = FMath::Lerp(2.25, 1.5, FMath::Clamp((Elapsed - 10.0) / 14.0, 0.0, 1.0));
    const TArray<double>& Notes = RunChart();
    for (int32 Index = RunNote; Index < Notes.Num(); ++Index)
    {
        const double Until = Notes[Index] - Elapsed;
        if (Until > ApproachTime) { break; }
        const double X = 70 + Until / ApproachTime * 386;
        const double Y = 51 + (Index % 2) * 37;
        Rect(X - 8, Y, 16, 20, Index % 2 == 0 ? Teal : Amber);
    }
    FString Hint = TEXT("Alternate A / D at the line");
    if (Elapsed < Notes[0])
    {
        Hint = FString::Printf(TEXT("Get ready  %d"), FMath::Clamp(FMath::CeilToInt((Notes[0] - Elapsed) / Beat), 1, ReadyBeats));
    }
    else if (Now < RunFeedbackUntil)
    {
        Hint = RunFeedback;
    }
    Text(Hint, 15, 134, Ink);
    Text(TEXT("Quit [Q]"), 407, 134, Ink);
}
