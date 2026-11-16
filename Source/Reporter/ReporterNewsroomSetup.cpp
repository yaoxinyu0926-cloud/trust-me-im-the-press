#include "ReporterGameMode.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

namespace
{
const FLinearColor Ink(0.047f, 0.031f, 0.019f);
const FLinearColor Cream(0.98f, 0.89f, 0.72f);
const FLinearColor Teal(0.019f, 0.115f, 0.102f);
}

void AReporterHUD::BeginNewsroomSetup()
{
    // A new game begins with empty floor space and the starter items available in Decorate.
    bHasDesk = false;
    bHasBed = false;
    bBedSelected = false;
    DeskPosition = FVector2D(0.30, 0.85);
    BedPosition = FVector2D(0.76, 0.86);
    ComputerPosition = FVector2D(0, -0.25);
    DeskScale = BedScale = 1.0;
    ComputerScale = 1.20;
    DeskTurn = BedTurn = 0;
    BedAngle = 0;
    bNewsroomSetupGuide = true;
    bFirstStoryGuide = false;
    bShowFirstStoryResults = false;
    FirstStoryThoughtUntil = 0.0;
    LatestCapturedFountainPhoto.Reset();
}

bool AReporterHUD::IsNearEditorialBed() const
{
    if (!bHasBed || !BedBounds.bIsValid || bDecorating)
    {
        return false;
    }
    const FVector2D Radius = GetBedRadius();
    const FVector2D BedCenter = BedPosition - FVector2D(0, Radius.Y);
    return ((EditorialPosition - BedCenter) / (Radius + FVector2D(0.065, 0.07))).SizeSquared() <= 1.0;
}

void AReporterHUD::RestAtEditorialBed(bool bUntilMorning)
{
    if (!bInEditorial || !IsNearEditorialBed())
    {
        return;
    }
    const int32 Restored = FMath::Clamp(100 - ReporterStamina, 0, bUntilMorning ? 100 : RestStamina);
    ReporterStamina += Restored;
    if (bUntilMorning)
    {
        const double Morning = GetGameDay() * 1440.0 + 8 * 60;
        AdvanceGameMinutes(Morning - OutdoorGameMinutes);
        if (bEndingOpen && EndingIssue.Kind != EEndingKind::Special) { return; }
        // The special interrupts midnight processing in the same new day.
        OutdoorGameMinutes = Morning;
        EditorialRestNotice = TEXT("8:00 AM  |  Stamina full");
    }
    else
    {
        const int32 Minutes = FurnitureCost(EFurniture::Bed, RestMinutes);
        AdvanceGameMinutes(Minutes);
        EditorialRestNotice = Restored > 0
            ? FString::Printf(TEXT("+%d Stamina  |  %s passed"), Restored, *FurnitureDuration(Minutes))
            : FString::Printf(TEXT("%s passed  |  Stamina full"), *FurnitureDuration(Minutes));
    }
    EditorialRestNoticeUntil = GetWorld()->GetRealTimeSeconds() + 2.5;
}

void AReporterHUD::DrawBedRestAction(FVector2D TopLeft, FVector2D Size)
{
    const double S = Size.Y / 720.0;
    const double Margin = 179 * S;
    const double X = FMath::Clamp(BedBounds.GetCenter().X, TopLeft.X + Margin, TopLeft.X + Size.X - Margin);
    const double Y = BedBounds.Min.Y - 39 * S;
    UFont* Font = TitleBoldFont ? TitleBoldFont.Get() : GEngine->GetMediumFont();
    const float TextScale = 15 * S / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size);
    const auto Button = [&](const FString& Label, double Left, double Width)
    {
        const FVector2D Min(X + Left * S, Y);
        const FVector2D Extent(Width * S, 32 * S);
        DrawRect(Ink, Min.X, Min.Y, Extent.X, Extent.Y);
        DrawRect(Cream, Min.X + 2 * S, Min.Y + 2 * S, Extent.X - 4 * S, Extent.Y - 4 * S);
        float W, H;
        MeasureUiText(Label, W, H, Font, TextScale);
        DrawUiText(Label, Teal, Min.X + (Extent.X - W) * 0.5, Min.Y + (Extent.Y - H) * 0.5, Font, TextScale);
        return FBox2D(Min, Min + Extent);
    };
    Interaction = EInteraction::RestAtBed;
    InteractionButton = InteractionMarker = Button(FString::Printf(TEXT("Rest %s"),
        *FurnitureDuration(FurnitureCost(EFurniture::Bed, RestMinutes))), -174, 144);
    MorningRestButton = Button(TEXT("Rest Until Morning"), -22, 196);
}

void AReporterHUD::DrawMorningRestConfirmation()
{
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const double S = FMath::Min(View.X / 700.0, View.Y / 460.0);
    const FVector2D Origin = (View - FVector2D(498, 208) * S) * 0.5;
    UFont* Body = GEngine->GetMediumFont();
    UFont* Heading = TitleBoldFont ? TitleBoldFont.Get() : Body;
    const auto Text = [&](const FString& Value, double X, double Y, float Size, UFont* Font, FLinearColor Color)
    {
        const FVector2D P = Origin + FVector2D(X, Y) * S;
        DrawUiText(Value, Color, P.X, P.Y, Font,
            Size * S / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size));
    };
    const auto Button = [&](const TCHAR* Label, double X, double W, FLinearColor Color)
    {
        const FVector2D Min = Origin + FVector2D(X, 153) * S;
        const FVector2D Extent(W * S, 34 * S);
        DrawRect(Color, Min.X, Min.Y, Extent.X, Extent.Y);
        Text(Label, X + 12, 160, 14, Body, Cream);
        return FBox2D(Min, Min + Extent);
    };
    DrawRect(FLinearColor(0.008f, 0.021f, 0.025f), 0, 0, View.X, View.Y);
    DrawRect(Ink, Origin.X, Origin.Y, 498 * S, 208 * S);
    DrawRect(Cream, Origin.X + 3 * S, Origin.Y + 3 * S, 492 * S, 202 * S);
    Text(TEXT("Rest Until Morning"), 20, 18, 25, Heading, Teal);
    Text(FString::Printf(TEXT("Wake up on Day %d at 8:00 AM with full stamina."), GetGameDay() + 1),
        21, 67, 15, Body, Ink);
    if (GetGameDay() % RentInterval == 0)
    {
        Text(FString::Printf(TEXT("Rent of $%d is due tonight. Current cash: $%d."), RentAmount, ReporterCash),
            21, 99, 15, Body, Ink);
    }
    else if (GetGameDay() == 30 && EndingIssue.Kind == EEndingKind::None)
    {
        Text(TEXT("The Day 30 special edition will open overnight."), 21, 99, 15, Body, Ink);
    }
    MorningRestCancel = Button(TEXT("Cancel"), 21, 90, Ink);
    MorningRestConfirm = Button(TEXT("Sleep until 8:00 AM"), 302, 175, Teal);
}

void AReporterHUD::ClickMorningRestConfirmation(bool bUsePointer)
{
    if (bUsePointer)
    {
        float X, Y;
        if (!GetOwningPlayerController()->GetMousePosition(X, Y)) { return; }
        const FVector2D Pointer(X, Y);
        if (MorningRestCancel.bIsValid && MorningRestCancel.IsInsideOrOn(Pointer))
        {
            bConfirmMorningRest = false;
            return;
        }
        if (!MorningRestConfirm.bIsValid || !MorningRestConfirm.IsInsideOrOn(Pointer)) { return; }
    }
    bConfirmMorningRest = false;
    RestAtEditorialBed(true);
}

void AReporterHUD::DrawNewsroomSetupGuide(FVector2D TopLeft, FVector2D Size)
{
    SkipSetupGuideButton = FBox2D(ForceInit);
    const double S = Size.Y / 720.0;
    const auto Card = [&](FVector2D Min, FVector2D Extent, FLinearColor Fill)
    {
        DrawRect(Ink, Min.X, Min.Y, Extent.X, Extent.Y);
        DrawRect(Fill, Min.X + 2 * S, Min.Y + 2 * S, Extent.X - 4 * S, Extent.Y - 4 * S);
    };
    UFont* HeadingFont = TitleBoldFont ? TitleBoldFont.Get() : GEngine->GetMediumFont();
    const auto Text = [&](const FString& Value, FVector2D P, UFont* Font, float SizePoints, FLinearColor Color)
    {
        const float TextScale = SizePoints * S / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size);
        DrawUiText(Value, Color, P.X, P.Y, Font, TextScale);
    };
    if (GetWorld()->GetRealTimeSeconds() < EditorialRestNoticeUntil && !bDecorating)
    {
        const FVector2D Min = TopLeft + FVector2D(0.5 * Size.X - 140 * S, 29 * S);
        Card(Min, FVector2D(280, 39) * S, Cream);
        Text(EditorialRestNotice, Min + FVector2D(14, 9) * S, HeadingFont, 14, Teal);
    }
    if (bNewsroomSetupGuide && bHasDesk && bHasBed && !bDecorating && !HasIntroUnreadMail() && bSponsorshipClaimed)
    {
        bNewsroomSetupGuide = false;
        bFirstStoryGuide = !bFountainReported && (!bToyReturned || !LatestCapturedFountainPhoto.IsEmpty());
    }
    if (!bNewsroomSetupGuide)
    {
        return;
    }

    const double HudScale = GetHudScale();
    const double ControlsY = bReporterPanelOpen ? 327 : 95;
    const FVector2D SkipMin = FVector2D(14, ControlsY + (bDecorating ? 69 : 42)) * HudScale;
    const FVector2D SkipExtent(155 * HudScale, 33 * HudScale);
    SkipSetupGuideButton = FBox2D(SkipMin, SkipMin + SkipExtent);
    Card(SkipMin, SkipExtent, Teal);
    Text(TEXT("Skip Tutorial"), SkipMin + FVector2D(12, 7) * S, HeadingFont, 13, Cream);

    const bool bReadyForMail = bHasDesk && bHasBed && !bDecorating;
    const TCHAR* Heading = TEXT("Your Newsroom");
    const TCHAR* Body = TEXT("This is your newsroom now.\nPress E or click Decorate to set it up.");
    const FBox2D* Highlight = &DecorButton;
    if (bReadyForMail)
    {
        Heading = TEXT("Check Your Mail");
        Body = TEXT("Click the computer to check your mail.\nRest in bed to restore Stamina.");
        Highlight = &ComputerBounds;
    }
    else if (bDecorating && !bHasDesk)
    {
        Heading = TEXT("Set Up Your Desk");
        Body = TEXT("Select Old Desk and drag it into place.\nYour computer comes with the desk.");
        Highlight = &DeskButton;
    }
    else if (!bHasBed && bHasDesk)
    {
        Heading = TEXT("Place a Bed");
        Body = bDecorating ? TEXT("Drag the desk into place, then place\nyour Old Bed where you have room.")
            : TEXT("Open Decorate again and place\nyour Old Bed where you have room.");
        Highlight = bDecorating ? &BedButton : &DecorButton;
    }
    else if (bDecorating)
    {
        Heading = TEXT("Finish Decorating");
        Body = TEXT("Drag the bed where you want it.\nPress R to rotate, or click Done.");
        Highlight = &DoneButton;
    }

    if (Highlight && Highlight->bIsValid)
    {
        const FBox2D Edge = Highlight->ExpandBy(3 * S);
        const double Thickness = FMath::Max(1.0, 2 * S);
        DrawLine(Edge.Min.X, Edge.Min.Y, Edge.Max.X, Edge.Min.Y, Cream, Thickness);
        DrawLine(Edge.Min.X, Edge.Max.Y, Edge.Max.X, Edge.Max.Y, Cream, Thickness);
        DrawLine(Edge.Min.X, Edge.Min.Y, Edge.Min.X, Edge.Max.Y, Cream, Thickness);
        DrawLine(Edge.Max.X, Edge.Min.Y, Edge.Max.X, Edge.Max.Y, Cream, Thickness);
    }

    const FVector2D CardExtent(335 * S, 107 * S);
    const double X = TopLeft.X + Size.X - CardExtent.X - 44 * S;
    const double Y = TopLeft.Y + 107 * S;
    const FVector2D Min(X, Y);
    Card(Min, CardExtent, Cream);
    Text(Heading, Min + FVector2D(15, 10) * S, HeadingFont, 20, Teal);
    DrawLine(X + 15 * S, Y + 43 * S, X + CardExtent.X - 15 * S, Y + 43 * S, Teal, 1.0f);
    Text(Body, Min + FVector2D(15, 52) * S, GEngine->GetMediumFont(), 13, Ink);
}

void AReporterHUD::DrawFirstStoryGuide()
{
    if (!bFirstStoryGuide || bDecorating || bReporterPanelOpen)
    {
        return;
    }
    const bool bHavePhoto = !LatestCapturedFountainPhoto.IsEmpty();
    const bool bShowThought = !bInLobby && bHavePhoto &&
        GetWorld()->GetRealTimeSeconds() < FirstStoryThoughtUntil;
    const TCHAR* Hint;
    if (bShowThought)
    {
        Hint = TEXT("Just a toy dinosaur. Nothing unusual...\nIs there a story here? Let's go back and find out.");
    }
    else if (bHavePhoto)
    {
        Hint = bInEditorial ? TEXT("Click the computer to edit your story.")
            : bInLobby ? TEXT("Head to Editorial to write your story.")
            : TEXT("Take the photo back to the newsroom.");
    }
    else
    {
        Hint = bInLobby ? TEXT("Head outside and look for a story in the park.")
            : FountainQuestion.bIsValid ? TEXT("Click the ? to photograph the fountain.")
            : ToyPlace == EItemPlace::Backpack
                ? TEXT("Set the toy back by the fountain to photograph it.")
                : TEXT("Look for a story in the park.");
    }

    const double S = GetHudScale();
    UFont* Font = GEngine->GetMediumFont();
    const float TextScale = 14 * S / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size);
    float Width = 0;
    float Height = 0;
    MeasureUiText(Hint, Width, Height, Font, TextScale);
    const FVector2D Extent(Width + 28 * S, Height + 18 * S);
    const FVector2D Min((Canvas->ClipX - Extent.X) * 0.5, 92 * S);
    DrawRect(Ink, Min.X, Min.Y, Extent.X, Extent.Y);
    DrawRect(Cream, Min.X + 2 * S, Min.Y + 2 * S, Extent.X - 4 * S, Extent.Y - 4 * S);
    DrawUiText(Hint, Teal, Min.X + 14 * S, Min.Y + 9 * S, Font, TextScale);
}
