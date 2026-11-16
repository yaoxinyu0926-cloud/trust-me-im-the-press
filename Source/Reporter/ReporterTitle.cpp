#include "ReporterGameMode.h"

#include "CanvasItem.h"
#include "Components/AudioComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PaperSprite.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"

namespace
{
constexpr float ViewfinderVolume = 0.12f;
constexpr float NewspaperVolume = 0.20f;
constexpr float ShutterDuration = 0.12f;
constexpr double ZoomStart = 0.43;
constexpr double ZoomEnd = 1.58;
constexpr double PaperStart = 2.38;
constexpr double PaperEnd = 3.24;
constexpr double ComicZoomEnd = 0.90;
constexpr double ComicCaptionEnd = 1.18;
const TCHAR* ComicCaptions[] = {
    TEXT("Every day, a thousand stories unfold in this city."),
    TEXT("All you need is a camera, a good story..."),
    TEXT("...and a place to call your own."),
    TEXT("Oh, and rent is due in seven days.")
};
const FVector2D CropOrigin(0.1527, 0);
const FVector2D CropSize(0.6766, 0.6970);
const FLinearColor Backdrop(0.002f, 0.021f, 0.031f);
const FLinearColor Ink(0.027f, 0.018f, 0.012f);
const FLinearColor Cream(0.97f, 0.84f, 0.64f);
const FLinearColor Teal(0.012f, 0.071f, 0.078f);

double Ease(double Time, double Start, double End)
{
    const double Alpha = FMath::Clamp((Time - Start) / (End - Start), 0.0, 1.0);
    return Alpha * Alpha * (3.0 - 2.0 * Alpha);
}
}

AReporterHUD::AReporterHUD()
{
    static ConstructorHelpers::FObjectFinder<UFont> Bold(
        TEXT("/Game/Reporter/RuntimeArt/DejaVuSerif-Bold_Font.DejaVuSerif-Bold_Font"));
    static ConstructorHelpers::FObjectFinder<UFont> Italic(
        TEXT("/Game/Reporter/RuntimeArt/DejaVuSerif-Italic_Font.DejaVuSerif-Italic_Font"));
    TitleBoldFont = Bold.Object;
    TitleItalicFont = Italic.Object;
    static ConstructorHelpers::FObjectFinder<USoundWave> Music(
        TEXT("/Game/Reporter/Audio/MutedTrumpetGroove.MutedTrumpetGroove"));
    BackgroundMusic = Music.Object;
    static ConstructorHelpers::FObjectFinder<USoundWave> SpecialMusic(
        TEXT("/Game/Reporter/Audio/FourNoteMotif.FourNoteMotif"));
    static ConstructorHelpers::FObjectFinder<USoundWave> FailedMusic(
        TEXT("/Game/Reporter/Audio/FinalMajorSixth.FinalMajorSixth"));
    SpecialEndingMusic = SpecialMusic.Object;
    FailedEndingMusic = FailedMusic.Object;
}

void AReporterHUD::InitTitle()
{
    const FString Art = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt"));
    TitleRescue = FImageUtils::ImportFileAsTexture2D(Art / TEXT("TitleRescue.jpg"));
    TitleNewsprint = FImageUtils::ImportFileAsTexture2D(Art / TEXT("TitleNewsprint.jpg"));
    if (bAssembleOpeningComic)
    {
        OpeningComicPage = FImageUtils::ImportFileAsTexture2D(Art / TEXT("OpeningComicPage.jpg"));
    }
    if (!OpeningComicPage)
    {
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(ComicCaptions); ++Index)
        {
            UTexture2D* Panel = FImageUtils::ImportFileAsTexture2D(
                Art / FString::Printf(TEXT("OpeningComic%d.jpg"), Index + 1));
            if (!Panel)
            {
                UE_LOG(LogTemp, Warning, TEXT("Could not load opening comic panel %d."), Index + 1);
                OpeningComicArt.Reset();
                break;
            }
            OpeningComicArt.Add(Panel);
        }
    }
    if (TitleRescue)
    {
        TitlePage = ETitlePage::Viewfinder;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Title rescue artwork missing; opening the street directly."));
    }
    if (BackgroundMusic)
    {
        BackgroundMusicPlayer = UGameplayStatics::CreateSound2D(this, BackgroundMusic);
        if (BackgroundMusicPlayer)
        {
            BackgroundMusicPlayer->SetUISound(true);
            ApplyAudioSettings();
            BackgroundMusicPlayer->FadeIn(0.03f, IsTitleOpen() ? ViewfinderVolume : GameplayMusicVolume);
        }
    }
}

void AReporterHUD::PlayShutterSound()
{
    TArray<uint8> Samples;
    const FString Path = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt/TitleShutter.pcm");
    if (FFileHelper::LoadFileToArray(Samples, *Path) && Samples.Num() == 5760)
    {
        USoundWaveProcedural* Shutter = NewObject<USoundWaveProcedural>(this);
        Shutter->SetSampleRate(24000);
        Shutter->NumChannels = 1;
        Shutter->Duration = ShutterDuration;
        Shutter->QueueAudio(Samples.GetData(), Samples.Num());
        if (UAudioComponent* Sound = UGameplayStatics::SpawnSound2D(this, Shutter, EffectsVolume))
        {
            Sound->FadeOut(0.18f, 0.0f);
        }
    }
}

void AReporterHUD::StopBackgroundMusic()
{
    if (IsValid(BackgroundMusicPlayer))
    {
        BackgroundMusicPlayer->Stop();
    }
    BackgroundMusicPlayer = nullptr;
}

void AReporterHUD::HandleTitleClick(bool bUsePointer)
{
    if (TitlePage == ETitlePage::CreateReporter)
    {
        return;
    }
    if (TitlePage == ETitlePage::Comic && OpeningComicPage)
    {
        if (bUsePointer)
        {
            AdvanceOpeningComic();
        }
        return;
    }
    int32 Selection = 0;
    if (bUsePointer)
    {
        const APlayerController* Player = GetOwningPlayerController();
        float X, Y;
        if (!Player || !Player->GetMousePosition(X, Y))
        {
            return;
        }
        Selection = INDEX_NONE;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(TitleButtons); ++Index)
        {
            if (TitleButtons[Index].bIsValid && TitleButtons[Index].IsInsideOrOn(FVector2D(X, Y)))
            {
                Selection = Index;
                break;
            }
        }
    }
    if (Selection == INDEX_NONE)
    {
        return;
    }

    switch (TitlePage)
    {
    case ETitlePage::Viewfinder:
    {
        TitlePage = ETitlePage::Reveal;
        TitleRevealStart = GetWorld()->GetRealTimeSeconds();
        PlayShutterSound();
        if (IsValid(BackgroundMusicPlayer))
        {
            FTimerHandle VolumeChange;
            GetWorldTimerManager().SetTimer(VolumeChange, FTimerDelegate::CreateWeakLambda(this, [this]
            {
                if (IsTitleOpen() && IsValid(BackgroundMusicPlayer))
                {
                    BackgroundMusicPlayer->AdjustVolume(0.20f, NewspaperVolume);
                }
            }), ShutterDuration, false);
        }
        break;
    }
    case ETitlePage::Menu:
        if (Selection == 0)
        {
            if (!OpeningComicPage && OpeningComicArt.IsEmpty())
            {
                BeginReporterCreation();
            }
            else
            {
                TitlePage = ETitlePage::Comic;
                OpeningComicPanel = 0;
                OpeningPanelStarted = GetWorld()->GetRealTimeSeconds();
            }
        }
        else
        {
            TitlePage = Selection == 1 ? ETitlePage::Load : ETitlePage::Achievements;
            if (TitlePage == ETitlePage::Load)
            {
                SettingsFeedback.Reset();
                RefreshSaveSlots();
            }
        }
        break;
    case ETitlePage::Comic:
        AdvanceOpeningComic();
        break;
    case ETitlePage::Load:
    case ETitlePage::Achievements:
        TitlePage = ETitlePage::Menu;
        break;
    default:
        return;
    }
    for (FBox2D& Button : TitleButtons)
    {
        Button = FBox2D(ForceInit);
    }
}

void AReporterHUD::DrawTitle()
{
    for (FBox2D& Button : TitleButtons)
    {
        Button = FBox2D(ForceInit);
    }

    if (TitlePage == ETitlePage::CreateReporter)
    {
        DrawReporterCreation();
        return;
    }
    if (TitlePage == ETitlePage::Comic)
    {
        if (OpeningComicPage)
        {
            DrawAssembledOpeningComic();
        }
        else
        {
            DrawOpeningComic();
        }
        return;
    }
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const double Scale = FMath::Min(View.X / 1600.0, View.Y / 900.0);
    const FVector2D Origin = (View - FVector2D(1600, 900) * Scale) * 0.5;
    const auto At = [&](FVector2D P) { return Origin + P * Scale; };
    const auto Rect = [&](FBox2D Box, FLinearColor Color)
    {
        const FVector2D P = At(Box.Min);
        const FVector2D Size = Box.GetSize() * Scale;
        DrawRect(Color, P.X, P.Y, Size.X, Size.Y);
    };
    const auto Image = [&](UTexture2D* Texture, FBox2D Box, FVector2D UV, FVector2D Span, float Alpha = 1.0f)
    {
        const FVector2D P = At(Box.Min);
        const FVector2D Size = Box.GetSize() * Scale;
        DrawTexture(Texture, P.X, P.Y, Size.X, Size.Y, UV.X, UV.Y, Span.X, Span.Y,
            FLinearColor(1, 1, 1, Alpha), Alpha < 1.0f ? BLEND_Translucent : BLEND_Opaque);
    };
    const auto Text = [&](const TCHAR* Label, FVector2D Center, float Size, FLinearColor Color, bool bItalic = false)
    {
        UFont* Font = bItalic ? TitleItalicFont.Get() : TitleBoldFont.Get();
        if (!Font)
        {
            Font = GEngine->GetMediumFont();
        }
        const float TextScale = Size * Scale / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size);
        float Width = 0.0f, Height = 0.0f;
        MeasureUiText(Label, Width, Height, Font, TextScale);
        const FVector2D Position = At(Center) - FVector2D(Width, Height) * 0.5;
        DrawUiText(Label, Color, Position.X, Position.Y, Font, TextScale);
    };

    float MouseX = -1, MouseY = -1;
    if (const APlayerController* Player = GetOwningPlayerController())
    {
        Player->GetMousePosition(MouseX, MouseY);
    }
    const auto Button = [&](int32 Index, FBox2D Box, const TCHAR* Label, bool bPrimary)
    {
        TitleButtons[Index] = FBox2D(At(Box.Min), At(Box.Max));
        const bool bHover = TitleButtons[Index].IsInsideOrOn(FVector2D(MouseX, MouseY));
        Rect(Box, bPrimary || bHover ? Teal : Ink);
        if (!bPrimary && !bHover)
        {
            Rect(Box.ExpandBy(-2.0), Cream);
        }
        Text(Label, Box.GetCenter(), 25, bPrimary || bHover ? Cream : Ink);
    };

    DrawRect(Backdrop, 0, 0, View.X, View.Y);
    const double Elapsed = GetWorld()->GetRealTimeSeconds() - TitleRevealStart;
    if (TitlePage == ETitlePage::Reveal && Elapsed >= PaperEnd)
    {
        TitlePage = ETitlePage::Menu;
    }

    const FBox2D PaperBounds(FVector2D(144, 18), FVector2D(1456, 882));
    const FBox2D Photo(FVector2D(252, 165), FVector2D(1348, 165 + 1096.0 * 941.0 / 1672.0));
    const auto DrawPaper = [&](float Alpha)
    {
        if (TitleNewsprint)
        {
            Image(TitleNewsprint, PaperBounds, FVector2D::ZeroVector, FVector2D::UnitVector, Alpha);
        }
        else
        {
            Rect(PaperBounds, FLinearColor(Cream.R, Cream.G, Cream.B, Alpha));
        }
    };

    if (TitlePage == ETitlePage::Viewfinder || TitlePage == ETitlePage::Reveal)
    {
        const bool bTakePhoto = TitlePage == ETitlePage::Viewfinder;
        const double Zoom = bTakePhoto ? 0.0 : Ease(Elapsed, ZoomStart, ZoomEnd);
        const double Settle = bTakePhoto ? 0.0 : Ease(Elapsed, PaperStart, PaperEnd);
        const FVector2D UV = CropOrigin * (1.0 - Zoom);
        const FVector2D Span = FMath::Lerp(CropSize, FVector2D::UnitVector, Zoom);
        const double Aspect = double(TitleRescue->GetSizeX()) / TitleRescue->GetSizeY() * Span.X / Span.Y;
        const FVector2D Extent(900 * Aspect, 900);
        const FVector2D Min = (FVector2D(1600, 900) - Extent) * 0.5;
        const FBox2D Frame(Min, Min + Extent);
        if (Settle > 0)
        {
            DrawPaper(Settle);
        }
        Image(TitleRescue, FBox2D(FMath::Lerp(Frame.Min, Photo.Min, Settle),
            FMath::Lerp(Frame.Max, Photo.Max, Settle)), UV, Span);

        if (bTakePhoto)
        {
            constexpr double Inset = 34;
            for (double XSign : {-1.0, 1.0})
            {
                for (double YSign : {-1.0, 1.0})
                {
                    const FVector2D Corner = Frame.GetCenter() + FVector2D(
                        XSign * (Frame.GetExtent().X - Inset), YSign * (Frame.GetExtent().Y - Inset));
                    const FVector2D P = At(Corner);
                    DrawLine(P.X, P.Y, P.X - XSign * 27 * Scale, P.Y, Cream, 2 * Scale);
                    DrawLine(P.X, P.Y, P.X, P.Y - YSign * 27 * Scale, Cream, 2 * Scale);
                }
            }
            Button(0, FBox2D(FVector2D(680, 801), FVector2D(920, 855)), TEXT("Take Photo"), true);
        }
        else if (Elapsed < 0.18)
        {
            DrawRect(FLinearColor(1, 1, 1, (1.0 - Elapsed / 0.18) * 0.92), 0, 0, View.X, View.Y);
        }
        return;
    }

    DrawPaper(1);
    Text(TEXT("Trust Me, I'm the Press!"), FVector2D(800, 77), TitleMastheadSize, Ink);
    Rect(FBox2D(FVector2D(252, 112), FVector2D(1348, 114)), Ink);
    Text(TEXT("Fake News... Or Is It?"), FVector2D(800, 137), TitleSubtitleSize, TitleSubtitleColor, true);

    if (TitlePage == ETitlePage::Menu)
    {
        Image(TitleRescue, Photo, FVector2D::ZeroVector, FVector2D::UnitVector);
        Button(0, FBox2D(FVector2D(252, 801), FVector2D(594, 853)), TEXT("Start Game"), false);
        Button(1, FBox2D(FVector2D(629, 801), FVector2D(971, 853)), TEXT("Load Game"), false);
        Button(2, FBox2D(FVector2D(1006, 801), FVector2D(1348, 853)), TEXT("Achievements"), false);
        return;
    }

    if (TitlePage == ETitlePage::Load) { return; }
    Text(TEXT("Achievements"), FVector2D(800, 310), 40, Ink);
    Text(TEXT("No achievements unlocked yet."),
        FVector2D(800, 386), 23, Ink);
    Button(0, FBox2D(FVector2D(252, 801), FVector2D(594, 853)), TEXT("Back"), true);
}

void AReporterHUD::AdvanceOpeningComic()
{
    const double Now = GetWorld()->GetRealTimeSeconds();
    if (Now - OpeningPanelStarted < ComicCaptionEnd)
    {
        return;
    }
    if (++OpeningComicPanel == (OpeningComicPage ? UE_ARRAY_COUNT(ComicCaptions) : OpeningComicArt.Num()))
    {
        BeginReporterCreation();
        return;
    }
    OpeningPanelStarted = Now;
}

void AReporterHUD::BeginNewsroomPlay()
{
    TitlePage = ETitlePage::Playing;
    OpeningComicPage = nullptr;
    OpeningComicArt.Reset();
    if (IsValid(BackgroundMusicPlayer))
    {
        BackgroundMusicPlayer->AdjustVolume(0.35f, GameplayMusicVolume);
    }
    if (!EditorialTexture || !LobbyTexture)
    {
        return;
    }

    // Keep the outdoor pawn at this building while its indoor position is drawn by the HUD.
    for (const AParkPropActor* Prop : ParkProps)
    {
        if (IsValid(Prop) && Prop->Artwork && Prop->Artwork->GetFName() == FName(TEXT("SP_NewsCenter")))
        {
            if (APawn* Pawn = GetOwningPawn())
            {
                const FVector2D Ground = Prop->GetBase();
                const FVector2D Artwork = Prop->GetDrawSize();
                Pawn->SetActorLocation(FVector(Ground.X + Artwork.X * 0.013, Ground.Y + 38.0,
                    Pawn->GetActorLocation().Z));
            }
            break;
        }
    }
    bInLobby = true;
    bInEditorial = true;
    EditorialPosition = FVector2D(0.55, 0.86);
    BeginNewsroomSetup();
    IndoorDirection = FVector2D(-1, 0);
}

void AReporterHUD::DrawOpeningComic()
{
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const double Scale = FMath::Min(View.X / 1600.0, View.Y / 900.0);
    const FVector2D Origin = (View - FVector2D(1600, 900) * Scale) * 0.5;
    const auto At = [&](double X, double Y) { return Origin + FVector2D(X, Y) * Scale; };
    const auto Rect = [&](FLinearColor Color, double X, double Y, double Width, double Height)
    {
        const FVector2D P = At(X, Y);
        DrawRect(Color, P.X, P.Y, Width * Scale, Height * Scale);
    };
    const auto Text = [&](const FString& Label, double X, double Y, float Size, FLinearColor Color, UFont* Font)
    {
        Font = Font ? Font : GEngine->GetMediumFont();
        const float TextScale = Size * Scale / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size);
        float Width, Height;
        MeasureUiText(Label, Width, Height, Font, TextScale);
        const FVector2D P = At(X, Y) - FVector2D(Width, Height) * 0.5;
        DrawUiText(Label, Color, P.X, P.Y, Font, TextScale);
    };

    DrawRect(Backdrop, 0, 0, View.X, View.Y);
    const double Age = GetWorld()->GetRealTimeSeconds() - OpeningPanelStarted;
    UTexture2D* Art = OpeningComicArt[OpeningComicPanel];
    const double Aspect = double(Art->GetSizeX()) / Art->GetSizeY();
    const double Height = FMath::Min(665.0, 1330.0 / Aspect);
    const double Width = Height * Aspect;
    const double X = (1600.0 - Width) * 0.5;
    const double Y = 39 + (665.0 - Height) * 0.5;
    Rect(Cream, X - 4, Y - 4, Width + 8, Height + 8);
    const FVector2D P = At(X, Y);
    // Zoom inside the panel so the paper edge and caption stay still.
    const double Span = 1.0 - 0.047 * Ease(Age, 0.0, ComicZoomEnd);
    const double Inset = (1.0 - Span) * 0.5;
    DrawTexture(Art, P.X, P.Y, Width * Scale, Height * Scale,
        Inset, Inset, Span, Span, FLinearColor::White, BLEND_Opaque);

    const float CaptionAlpha = Ease(Age, ComicZoomEnd, ComicCaptionEnd);
    if (CaptionAlpha <= 0.0f)
    {
        return;
    }
    Rect(Cream.CopyWithNewOpacity(CaptionAlpha), 250, 733, 1100, 63);
    Text(ComicCaptions[OpeningComicPanel], 800, 764, 26, Ink.CopyWithNewOpacity(CaptionAlpha), TitleItalicFont.Get());
    if (Age < ComicCaptionEnd)
    {
        return;
    }

    const bool bLastPanel = OpeningComicPanel + 1 == OpeningComicArt.Num();
    const FBox2D Button(At(674, 816), At(926, 862));
    TitleButtons[0] = Button;
    float MouseX, MouseY;
    const APlayerController* Player = GetOwningPlayerController();
    const bool bHover = Player && Player->GetMousePosition(MouseX, MouseY) &&
        Button.IsInsideOrOn(FVector2D(MouseX, MouseY));
    Rect(bHover ? Cream : Teal, 674, 816, 252, 46);
    Text(bLastPanel ? TEXT("Create Reporter") : TEXT("Next"), 800, 839, 23,
        bHover ? Ink : Cream, TitleBoldFont.Get());
}

void AReporterHUD::DrawAssembledOpeningComic()
{
    struct FPanel
    {
        FVector2D Corners[4];
        FBox2D Caption;
        const TCHAR* Lines;
    };
    static const FPanel Panels[] = {
        {{{0, 0}, {821, 0}, {785, 435}, {0, 435}},
            FBox2D({26, 23}, {471, 113}),
            TEXT("Every day, a thousand\nstories unfold in this city.")},
        {{{840, 0}, {1672, 0}, {1672, 435}, {803, 435}},
            FBox2D({863, 23}, {1329, 113}),
            TEXT("All you need is a camera,\na good story...")},
        {{{0, 449}, {678, 449}, {604, 941}, {0, 941}},
            FBox2D({25, 475}, {398, 565}),
            TEXT("...and a place\nto call your own.")},
        {{{697, 449}, {1672, 449}, {1672, 941}, {624, 941}},
            FBox2D({1244, 795}, {1646, 885}),
            TEXT("Oh, and rent is due\nin seven days.")}
    };
    static_assert(UE_ARRAY_COUNT(Panels) == UE_ARRAY_COUNT(ComicCaptions));

    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const FVector2D PageSize(1672, 941);
    const double Scale = FMath::Min(View.X / (PageSize.X + 84.0), View.Y / (PageSize.Y + 60.0));
    const FVector2D Origin = (View - PageSize * Scale) * 0.5;
    const FLinearColor PaperInk(0.97f, 0.88f, 0.72f);
    DrawRect(Backdrop, 0, 0, View.X, View.Y);
    DrawRect(PaperInk, Origin.X - 7 * Scale, Origin.Y - 7 * Scale,
        (PageSize.X + 14) * Scale, (PageSize.Y + 14) * Scale);

    const double CurrentAge = GetWorld()->GetRealTimeSeconds() - OpeningPanelStarted;
    TArray<FCanvasUVTri> Triangles;
    Triangles.Reserve((OpeningComicPanel + 1) * 2);
    for (int32 Index = 0; Index <= OpeningComicPanel; ++Index)
    {
        const FPanel& Panel = Panels[Index];
        const double Zoom = Index == OpeningComicPanel ? Ease(CurrentAge, 0.0, ComicZoomEnd) : 1.0;
        const FVector2D Center = (Panel.Corners[0] + Panel.Corners[1] + Panel.Corners[2] + Panel.Corners[3]) * 0.25;
        const auto UV = [&](int32 Corner)
        {
            return FMath::Lerp(Panel.Corners[Corner], Center, Zoom * 0.047) / PageSize;
        };
        const auto AddTriangle = [&](int32 A, int32 B, int32 C)
        {
            FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
            Triangle.V0_Pos = Origin + Panel.Corners[A] * Scale;
            Triangle.V1_Pos = Origin + Panel.Corners[B] * Scale;
            Triangle.V2_Pos = Origin + Panel.Corners[C] * Scale;
            Triangle.V0_UV = UV(A);
            Triangle.V1_UV = UV(B);
            Triangle.V2_UV = UV(C);
            Triangle.V0_Color = Triangle.V1_Color = Triangle.V2_Color = FLinearColor::White;
        };
        AddTriangle(0, 1, 2);
        AddTriangle(0, 2, 3);
    }
    FCanvasTriangleItem Item(Triangles, OpeningComicPage->GetResource());
    Item.BlendMode = SE_BLEND_Opaque;
    Canvas->DrawItem(Item);

    UFont* CaptionFont = TitleItalicFont ? TitleItalicFont.Get() : GEngine->GetMediumFont();
    const float TextScale = 24.0 * Scale / FMath::Max(1.0f, CaptionFont->GetLegacySlateFontInfo().Size);
    for (int32 Index = 0; Index <= OpeningComicPanel; ++Index)
    {
        const float Alpha = Index == OpeningComicPanel ? Ease(CurrentAge, ComicZoomEnd, ComicCaptionEnd) : 1.0f;
        if (Alpha <= 0.0f)
        {
            continue;
        }
        const FPanel& Panel = Panels[Index];
        const FVector2D P = Origin + Panel.Caption.Min * Scale;
        const FVector2D Extent = Panel.Caption.GetSize() * Scale;
        DrawRect(Ink.CopyWithNewOpacity(Alpha), P.X, P.Y, Extent.X, Extent.Y);
        DrawRect(PaperInk.CopyWithNewOpacity(Alpha), P.X + 3 * Scale, P.Y + 3 * Scale,
            Extent.X - 6 * Scale, Extent.Y - 6 * Scale);
        DrawUiText(Panel.Lines, Ink.CopyWithNewOpacity(Alpha), P.X + 16 * Scale, P.Y + 6 * Scale,
            CaptionFont, TextScale);
    }
}
