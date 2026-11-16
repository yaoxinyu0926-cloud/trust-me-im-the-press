#include "ReporterGameMode.h"

#include "Components/AudioComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
const TCHAR* AudioSection = TEXT("Reporter.Audio");
const FLinearColor Teal(0.08f, 0.28f, 0.28f);
const FLinearColor Cream(0.97f, 0.90f, 0.76f);
const FLinearColor PaperFill(0.99f, 0.94f, 0.83f);
const FLinearColor Ink(0.14f, 0.089f, 0.05f);
const FLinearColor Rule(0.57f, 0.46f, 0.34f);
const FLinearColor Coral(0.69f, 0.30f, 0.20f);
const FLinearColor Disabled(0.72f, 0.64f, 0.53f);
const TCHAR* LocationLabels[] = {
    TEXT("Rivermere Street"), TEXT("Editorial Office"), TEXT("Gym"), TEXT("Pool"),
    TEXT("Hot Pot Restaurant"), TEXT("Sports Center"), TEXT("News Center"), TEXT("Rivermere Mall"),
    TEXT("Rivermere Hospital")
};
constexpr double SliderWidth = 242;
constexpr double SliderTop = 181;
constexpr double EffectsTop = 268;
}

void AReporterHUD::LoadAudioSettings()
{
    GConfig->GetFloat(AudioSection, TEXT("Music"), MusicVolume, GGameUserSettingsIni);
    GConfig->GetFloat(AudioSection, TEXT("Effects"), EffectsVolume, GGameUserSettingsIni);
    MusicVolume = FMath::Clamp(FMath::IsFinite(MusicVolume) ? MusicVolume : 1.0f, 0.0f, 1.0f);
    EffectsVolume = FMath::Clamp(FMath::IsFinite(EffectsVolume) ? EffectsVolume : 1.0f, 0.0f, 1.0f);
}

void AReporterHUD::StoreAudioSettings() const
{
    GConfig->SetFloat(AudioSection, TEXT("Music"), MusicVolume, GGameUserSettingsIni);
    GConfig->SetFloat(AudioSection, TEXT("Effects"), EffectsVolume, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

void AReporterHUD::ApplyAudioSettings()
{
    if (IsValid(BackgroundMusicPlayer)) { BackgroundMusicPlayer->SetVolumeMultiplier(MusicVolume); }
    if (IsValid(ActivityMusicPlayer)) { ActivityMusicPlayer->SetVolumeMultiplier(MusicVolume); }
}

void AReporterHUD::ToggleSettings()
{
    if (IsMallShopOpen())
    {
        bFurnitureShopOpen = bFoodShopOpen = false;
        return;
    }
    if (bConfirmMorningRest)
    {
        bConfirmMorningRest = false;
        return;
    }
    if (bSettingsOpen || TitlePage == ETitlePage::Load)
    {
        if (SettingsConfirmAction != ESettingsAction::Close)
        {
            SettingsConfirmAction = ESettingsAction::Close;
            SettingsConfirmSlot = INDEX_NONE;
            return;
        }
        CloseSettings();
        return;
    }
    if (IsTitleOpen() || bEndingOpen || !PendingPhoto.IsEmpty()) { return; }

    ReleasePointer();
    HideStoryInputs();
    bSettingsOpen = true;
    bSettingsPausedWorld = !UGameplayStatics::IsGamePaused(this) && UGameplayStatics::SetGamePaused(this, true);
    SettingsFeedback.Reset();
    RefreshSaveSlots();
}

void AReporterHUD::CloseSettings()
{
    if (SettingsSlider != ESettingsAction::Close) { StoreAudioSettings(); }
    SettingsSlider = ESettingsAction::Close;
    SettingsConfirmSlot = INDEX_NONE;
    SettingsConfirmAction = ESettingsAction::Close;
    bSettingsOpen = false;
    if (bSettingsPausedWorld)
    {
        UGameplayStatics::SetGamePaused(this, false);
        bSettingsPausedWorld = false;
    }
    if (TitlePage == ETitlePage::Load) { TitlePage = ETitlePage::Menu; }
    SettingsButtons.Reset();
}

void AReporterHUD::DrawSettingsShortcut()
{
    if (SettingsShortcut.bIsValid || bCameraMode || !PendingPhoto.IsEmpty()) { return; }
    const double X = Canvas->ClipX / GetHudScale() * 0.5 - 47.5;
    SettingsShortcut = DrawHudShortcut(FVector2D(X, 13), false, true);
}

void AReporterHUD::UpdateSettingsSlider(float X)
{
    if (!SettingsSliderTrack.bIsValid) { return; }
    const float Volume = FMath::Clamp(float((X - SettingsSliderTrack.Min.X) / SettingsSliderTrack.GetSize().X), 0.0f, 1.0f);
    if (SettingsSlider == ESettingsAction::Music)
    {
        MusicVolume = Volume;
        ApplyAudioSettings();
    }
    else if (SettingsSlider == ESettingsAction::Effects)
    {
        EffectsVolume = Volume;
    }
}

void AReporterHUD::DrawSettings()
{
    const bool bTitleLoad = TitlePage == ETitlePage::Load;
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const double S = FMath::Min(View.X / 945.0, View.Y / 610.0);
    const FVector2D Origin = (View - FVector2D(880, 560) * S) * 0.5;
    SettingsButtons.Reset();
    const auto At = [&](double X, double Y) { return Origin + FVector2D(X, Y) * S; };
    const auto Rect = [&](double X, double Y, double W, double H, FLinearColor Color)
    {
        const FVector2D P = At(X, Y);
        DrawRect(Color, P.X, P.Y, W * S, H * S);
    };
    const auto Text = [&](const FString& Value, double X, double Y, float Points, FLinearColor Color,
        bool bBold = false, double MaxWidth = 0)
    {
        UFont* Font = bBold && TitleBoldFont ? TitleBoldFont.Get() : GEngine->GetMediumFont();
        float Scale = Points * S / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size);
        if (MaxWidth > 0)
        {
            float Width, Height;
            MeasureUiText(Value, Width, Height, Font, Scale);
            Scale *= FMath::Min(1.0, MaxWidth * S / FMath::Max(1.0f, Width));
        }
        const FVector2D P = At(X, Y);
        DrawUiText(Value, Color, P.X, P.Y, Font, Scale);
    };
    const auto Button = [&](ESettingsAction Action, int32 Slot, const FString& Label, double X, double Y,
        double W, FLinearColor Fill, bool bEnabled = true)
    {
        Rect(X, Y, W, 36, bEnabled ? Fill : Disabled);
        Text(Label, X + 10, Y + 7, 14, bEnabled ? PaperFill : Ink, true, W - 19);
        if (bEnabled) { SettingsButtons.Add({FBox2D(At(X, Y), At(X + W, Y + 36)), Action, Slot}); }
    };

    DrawRect(FLinearColor(0.025f, 0.020f, 0.018f, 0.56f), 0, 0, View.X, View.Y);
    if (SettingsConfirmAction != ESettingsAction::Close)
    {
        Rect(201, 168, 478, 211, Ink);
        Rect(204, 171, 472, 205, Cream);
        Rect(204, 171, 472, 47, Teal);
        const bool bSave = SettingsConfirmAction == ESettingsAction::Save;
        const bool bQuit = SettingsConfirmAction == ESettingsAction::Quit;
        Text(bQuit ? TEXT("Quit the game?") : bSave ? TEXT("Overwrite save?") : TEXT("Load this save?"),
            224, 178, 21, PaperFill, true);
        Text(bSave ? TEXT("This will replace the progress in this slot.")
            : TEXT("Your current unsaved progress will be lost."), 225, 241, 15, Ink, false, 430);
        Button(ESettingsAction::Cancel, INDEX_NONE, TEXT("Cancel"), 225, 314, 111, Teal);
        Button(ESettingsAction::Confirm, SettingsConfirmSlot, bQuit ? TEXT("Quit Game") : bSave ? TEXT("Overwrite") : TEXT("Load"),
            522, 314, 132, Coral);
        return;
    }

    if (SettingsSlider == ESettingsAction::Music || SettingsSlider == ESettingsAction::Effects)
    {
        const double Y = SettingsSlider == ESettingsAction::Music ? SliderTop : EffectsTop;
        SettingsSliderTrack = FBox2D(At(31, Y), At(31 + SliderWidth, Y + 14));
        float X, YPointer;
        if (GetOwningPlayerController()->GetMousePosition(X, YPointer)) { UpdateSettingsSlider(X); }
    }
    Rect(0, 0, 880, 560, Ink);
    Rect(2, 2, 876, 556, Cream);
    Rect(2, 2, 876, 59, Teal);
    Text(bTitleLoad ? TEXT("Load Game") : TEXT("Settings"), 30, 10, 25, PaperFill, true);
    Button(ESettingsAction::Close, INDEX_NONE, TEXT("X"), 824, 13, 38, Coral);
    Rect(316, 80, 1, 400, Rule);
    Rect(24, 490, 832, 1, Rule);

    Text(TEXT("Audio"), 31, 83, 21, Ink, true);
    Rect(31, 119, 242, 1, Rule);
    const auto Slider = [&](const TCHAR* Label, float Value, double Y, ESettingsAction Action)
    {
        Text(Label, 31, Y - 33, 16, Ink, true, 187);
        Text(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100)), 225, Y - 31, 12, Ink, true, 48);
        Rect(31, Y, SliderWidth, 12, Rule);
        Rect(32, Y + 1, SliderWidth - 2, 10, Disabled);
        if (Value > 0) { Rect(32, Y + 1, (SliderWidth - 2) * Value, 10, Coral); }
        Rect(26 + SliderWidth * Value, Y - 3, 10, 18, Ink);
        Rect(27 + SliderWidth * Value, Y - 2, 8, 16, PaperFill);
        SettingsButtons.Add({FBox2D(At(26, Y - 10), At(278, Y + 29)), Action, INDEX_NONE});
    };
    Slider(TEXT("Music"), MusicVolume, SliderTop, ESettingsAction::Music);
    Slider(TEXT("Sound Effects"), EffectsVolume, EffectsTop, ESettingsAction::Effects);
    Rect(31, 319, 242, 1, Rule);
    Text(TEXT("Language"), 31, 333, 19, Ink, true);
    Rect(31, 379, 242, 36, FLinearColor(0.83f, 0.75f, 0.62f));
    Text(TEXT("English"), 44, 385, 15, Ink);
    Text(TEXT("More languages coming later."), 32, 428, 11, Ink, false, 242);

    Text(bTitleLoad ? TEXT("Saved Games") : TEXT("Save & Load"), 339, 83, 21, Ink, true);
    Rect(339, 119, 514, 1, Rule);
    UTexture2D* Thumbnails[] = {
        DistantTown, EditorialTexture, GymTexture, PoolTexture, HotPotTexture, SportsLobbyTexture, LobbyTexture,
        MallInterior, HospitalInterior
    };
    static_assert(UE_ARRAY_COUNT(Thumbnails) == UE_ARRAY_COUNT(LocationLabels));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(SaveSlots); ++Index)
    {
        const FSaveSlotPreview& Slot = SaveSlots[Index];
        const double Y = 136 + Index * 102;
        Rect(339, Y, 514, 90, Rule);
        Rect(340, Y + 1, 512, 88, PaperFill);
        Rect(353, Y + 13, 97, 64, Disabled);
        const int32 Location = FMath::Clamp<int32>(Slot.Location, 0, UE_ARRAY_COUNT(LocationLabels) - 1);
        if (Slot.bReadable && IsValid(Thumbnails[Location]))
        {
            UTexture2D* Texture = Thumbnails[Location];
            const double Aspect = double(Texture->GetSizeX()) / Texture->GetSizeY();
            const double Target = 97.0 / 64.0;
            const double U = FMath::Min(1.0, Target / Aspect);
            const double V = FMath::Min(1.0, Aspect / Target);
            const FVector2D P = At(353, Y + 13);
            DrawTexture(Texture, P.X, P.Y, 97 * S, 64 * S, (1 - U) / 2, (1 - V) / 2, U, V,
                FLinearColor::White, BLEND_Opaque);
        }

        Text(FString::Printf(TEXT("Slot %d"), Index + 1), 463, Y + 8, 17, Ink, true);
        if (Slot.bReadable)
        {
            const int32 Minutes = FMath::FloorToInt(Slot.Minutes);
            Text(FString::Printf(TEXT("Day %d  |  %02d:%02d"), Minutes / 1440 + 1, Minutes / 60 % 24, Minutes % 60),
                463, Y + 35, 12, Ink);
            Text(FString::Printf(TEXT("%s  |  %s"), LocationLabels[Location], *Slot.Name),
                463, Y + 58, 11, Ink, false, 189);
        }
        else
        {
            Text(Slot.bOccupied ? TEXT("Unavailable") : TEXT("Empty"), 463, Y + 42, 13, Ink);
        }
        if (!bTitleLoad)
        {
            Button(ESettingsAction::Save, Index, TEXT("Save"), 665, Y + 28, 80, Coral, CanSaveAtCurrentLocation());
        }
        Button(ESettingsAction::Load, Index, TEXT("Load"), 756, Y + 28, 80, Teal, Slot.bReadable && PendingPhoto.IsEmpty());
    }

    Button(ESettingsAction::Close, INDEX_NONE, bTitleLoad ? TEXT("Back") : TEXT("Resume Game"), 27, 506, 180, Coral);
    if (!bTitleLoad)
    {
        Button(ESettingsAction::Quit, INDEX_NONE, TEXT("Quit Game"), 719, 506, 134, Coral);
    }
    if (!SettingsFeedback.IsEmpty())
    {
        Text(SettingsFeedback, 224, 511, 13, Teal, true, 475);
    }
    else if (!bTitleLoad && !CanSaveAtCurrentLocation())
    {
        Text(TEXT("Save after the current activity or transition."), 224, 511, 13, Ink, false, 475);
    }
}

bool AReporterHUD::ClickSettings(FVector2D Pointer)
{
    for (const FSettingsButton& Entry : SettingsButtons)
    {
        if (!Entry.Bounds.IsInsideOrOn(Pointer)) { continue; }
        const FSettingsButton Button = Entry;
        SettingsButtons.Reset();
        switch (Button.Action)
        {
        case ESettingsAction::Close:
            CloseSettings();
            break;
        case ESettingsAction::Quit:
            SettingsConfirmAction = ESettingsAction::Quit;
            SettingsConfirmSlot = INDEX_NONE;
            break;
        case ESettingsAction::Music:
        case ESettingsAction::Effects:
            SettingsSlider = Button.Action;
            SettingsSliderTrack = FBox2D(
                FVector2D(Button.Bounds.Min.X + Button.Bounds.GetSize().X * 5.0 / 252.0, Button.Bounds.Min.Y),
                FVector2D(Button.Bounds.Max.X - Button.Bounds.GetSize().X * 5.0 / 252.0, Button.Bounds.Max.Y));
            UpdateSettingsSlider(Pointer.X);
            break;
        case ESettingsAction::Save:
            if (SaveSlots[Button.Slot].bOccupied)
            {
                SettingsConfirmAction = Button.Action;
                SettingsConfirmSlot = Button.Slot;
            }
            else
            {
                SettingsFeedback = SaveReporterGame(Button.Slot) ? TEXT("Game saved.") : TEXT("Could not save the game.");
            }
            break;
        case ESettingsAction::Load:
            if (!IsTitleOpen())
            {
                SettingsConfirmAction = Button.Action;
                SettingsConfirmSlot = Button.Slot;
            }
            else if (!LoadReporterGame(Button.Slot))
            {
                SettingsFeedback = TEXT("This save could not be loaded.");
            }
            break;
        case ESettingsAction::Confirm:
        {
            const ESettingsAction Confirmed = SettingsConfirmAction;
            SettingsConfirmAction = ESettingsAction::Close;
            SettingsConfirmSlot = INDEX_NONE;
            if (Confirmed == ESettingsAction::Quit)
            {
                CloseSettings();
                UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
            }
            else if (Confirmed == ESettingsAction::Save)
            {
                SettingsFeedback = SaveReporterGame(Button.Slot) ? TEXT("Game saved.") : TEXT("Could not save the game.");
            }
            else if (Confirmed == ESettingsAction::Load && !LoadReporterGame(Button.Slot))
            {
                SettingsFeedback = TEXT("This save could not be loaded.");
            }
            break;
        }
        case ESettingsAction::Cancel:
            SettingsConfirmAction = ESettingsAction::Close;
            SettingsConfirmSlot = INDEX_NONE;
            break;
        default: break;
        }
        return true;
    }
    return false;
}
