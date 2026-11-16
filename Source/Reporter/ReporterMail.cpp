#include "ReporterGameMode.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"

namespace
{
constexpr int32 Sponsorship = 60;
constexpr int32 LeaseMail = 0;
constexpr int32 WelcomeMail = 1;
constexpr int32 ToyOwnerMail = 2;
constexpr int32 LostOwnerMail = 3;
struct FIntroMail
{
    const TCHAR* Subject;
    const TCHAR* Sender;
};
constexpr FIntroMail IntroMail[] = {
    {TEXT("Your Newsroom Lease"), TEXT("Property Office")},
    {TEXT("Welcome to Rivermere!"), TEXT("The Development Team")},
    {TEXT("About the green dinosaur"), TEXT("Jamie\u2019s Parent")}
};
const FLinearColor Ink(0.055f, 0.039f, 0.027f);
const FLinearColor Teal(0.025f, 0.118f, 0.131f);
const FLinearColor PaperColor(0.95f, 0.86f, 0.70f);
const FLinearColor LightPaper(0.985f, 0.93f, 0.81f);
const FLinearColor Selected(0.65f, 0.78f, 0.76f);
const FLinearColor Rule(0.46f, 0.39f, 0.29f);
}

bool AReporterHUD::HasIntroUnreadMail() const
{
    return !bMailRead[LeaseMail] || !bMailRead[WelcomeMail];
}

bool AReporterHUD::HasUnreadMail() const
{
    return HasIntroUnreadMail() || (HasToyOwnerMail() && !bToyOwnerMailRead) ||
        (HasLostOwnerMail() && !LostCases.Last().bMailRead);
}

void AReporterHUD::DrawMailUnreadDot(FVector2D Center, double Radius)
{
    TArray<FCanvasUVTri> Triangles;
    constexpr int32 Sides = 12;
    Triangles.Reserve(Sides);
    const FLinearColor Red(0.54f, 0.023f, 0.026f);
    for (int32 Index = 0; Index < Sides; ++Index)
    {
        const double A = Index * UE_TWO_PI / Sides;
        const double B = (Index + 1) * UE_TWO_PI / Sides;
        FCanvasUVTri& Tri = Triangles.AddDefaulted_GetRef();
        Tri.V0_Pos = Center;
        Tri.V1_Pos = Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius;
        Tri.V2_Pos = Center + FVector2D(FMath::Cos(B), FMath::Sin(B)) * Radius;
        Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Red;
        Tri.V0_UV = Tri.V1_UV = Tri.V2_UV = FVector2D::ZeroVector;
    }
    FCanvasTriangleItem Item(Triangles, Canvas->DefaultTexture->GetResource());
    Canvas->DrawItem(Item);
}

bool AReporterHUD::ClickMail(FVector2D Pointer)
{
    for (int32 Index = 0; Index < MailActions.Num(); ++Index)
    {
        if (!MailActions[Index].Key.IsInsideOrOn(Pointer))
        {
            continue;
        }
        const EMailAction Action = MailActions[Index].Value;
        MailActions.Reset();
        switch (Action)
        {
        case EMailAction::Inbox:
            MailPage = EMailPage::Inbox;
            break;
        case EMailAction::Sent:
            MailPage = EMailPage::Sent;
            break;
        case EMailAction::Contacts:
            MailPage = EMailPage::Contacts;
            break;
        case EMailAction::Compose:
            MailPage = EMailPage::Compose;
            break;
        case EMailAction::OpenLease:
        case EMailAction::OpenWelcome:
            SelectedMail = Action == EMailAction::OpenLease ? LeaseMail : WelcomeMail;
            bMailRead[SelectedMail] = true;
            break;
        case EMailAction::OpenToyOwner:
            if (HasToyOwnerMail())
            {
                SelectedMail = ToyOwnerMail;
                bToyOwnerMailRead = true;
            }
            break;
        case EMailAction::OpenLostOwner:
            if (HasLostOwnerMail())
            {
                SelectedMail = LostOwnerMail;
                LostCases.Last().bMailRead = true;
            }
            break;
        case EMailAction::Claim:
            if (!bSponsorshipClaimed && MailPage == EMailPage::Inbox && SelectedMail == WelcomeMail)
            {
                bSponsorshipClaimed = true;
                ReporterCash += Sponsorship;
            }
            break;
        }
        if (bNewsroomSetupGuide && bHasDesk && bHasBed && !HasIntroUnreadMail() && bSponsorshipClaimed)
        {
            bNewsroomSetupGuide = false;
            bFirstStoryGuide = !bFountainReported && (!bToyReturned || !LatestCapturedFountainPhoto.IsEmpty());
        }
        return true;
    }
    return false;
}

void AReporterHUD::DrawMail(FVector2D Origin, FVector2D Size, FVector2D Close)
{
    MailActions.Reset();
    const double S = Size.X / 800.0;
    const auto At = [&](double X, double Y) { return Origin + FVector2D(X, Y) * S; };
    const auto Rect = [&](double X, double Y, double W, double H, FLinearColor Color)
    {
        const FVector2D P = At(X, Y);
        DrawRect(Color, P.X, P.Y, W * S, H * S);
    };
    UFont* Regular = GEngine->GetMediumFont();
    UFont* Bold = TitleBoldFont ? TitleBoldFont.Get() : Regular;
    const auto Text = [&](const FString& Value, double X, double Y, float Points, FLinearColor Color, UFont* Font, double MaxWidth = 0)
    {
        const FVector2D P = At(X, Y);
        float Scale = Points * S / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size);
        if (MaxWidth > 0)
        {
            float Width, Height;
            MeasureUiText(Value, Width, Height, Font, Scale);
            Scale *= FMath::Min(1.0, MaxWidth * S / FMath::Max(1.0f, Width));
        }
        DrawUiText(Value, Color, P.X, P.Y, Font, Scale);
    };
    const auto Button = [&](EMailAction Action, const FString& Label, double X, double Y,
        double W, bool bFilled, bool bEnabled = true)
    {
        Rect(X, Y, W, 32, bFilled ? Teal : Rule);
        if (!bFilled)
        {
            Rect(X + 1, Y + 1, W - 2, 30, bEnabled ? LightPaper : PaperColor);
        }
        Text(Label, X + 10, Y + 6, 13, bFilled ? LightPaper : Ink, Bold);
        if (bEnabled)
        {
            MailActions.Emplace(FBox2D(At(X, Y), At(X + W, Y + 32)), Action);
        }
    };

    Rect(0, 0, 800, 600, Teal);
    Rect(7, 8, 786, 584, Ink);
    Rect(9, 10, 782, 580, PaperColor);
    Rect(10, 11, 780, 38, Teal);
    Text(TEXT("Rivermere Mail"), 24, 16, 20, LightPaper, Bold);
    Text(FString::Printf(TEXT("Cash $%d"), ReporterCash), 643, 19, 15, LightPaper, Bold);
    DrawRect(LightPaper, Close.X, Close.Y, 32 * S, 27 * S);
    Text(TEXT("X"), 764, 14, 14, Ink, Bold);
    Button(EMailAction::Compose, TEXT("Compose"), 20, 58, 117, false);
    Rect(10, 100, 780, 2, Rule);
    if (bNewsroomSetupGuide && MailPage == EMailPage::Inbox)
    {
        Text(HasIntroUnreadMail() ? TEXT("Read your two opening messages.") : TEXT("Open the welcome letter to claim $60."),
            154, 66, 13, Ink, Bold);
    }
    Rect(143, 102, 1, 488, Rule);
    Rect(423, 102, 1, 488, Rule);
    Rect(424, 102, 366, 488, LightPaper);

    struct FNavigation
    {
        EMailPage Page;
        EMailAction Action;
        const TCHAR* Label;
    };
    constexpr FNavigation Nav[] = {
        {EMailPage::Inbox, EMailAction::Inbox, TEXT("Inbox")},
        {EMailPage::Sent, EMailAction::Sent, TEXT("Sent")},
        {EMailPage::Contacts, EMailAction::Contacts, TEXT("Contacts")}
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Nav); ++Index)
    {
        const double Y = 116 + Index * 42;
        if (MailPage == Nav[Index].Page)
        {
            Rect(16, Y, 121, 36, Selected);
        }
        MailActions.Emplace(FBox2D(At(16, Y), At(137, Y + 36)), Nav[Index].Action);
        Text(Nav[Index].Label, 27, Y + 7, 15, Ink, Bold);
    }

    if (MailPage != EMailPage::Inbox)
    {
        switch (MailPage)
        {
        case EMailPage::Sent:
            Text(TEXT("Sent"), 448, 127, 23, Ink, Bold);
            Text(TEXT("No sent mail yet."), 449, 195, 14, Ink, Regular);
            break;
        case EMailPage::Contacts:
            Text(TEXT("Contacts"), 448, 127, 23, Ink, Bold);
            Text(TEXT("No contacts yet. Collect contact details\nduring interviews or investigations."),
                449, 195, 14, Ink, Regular);
            break;
        case EMailPage::Compose:
            Text(TEXT("New Message"), 448, 127, 23, Ink, Bold);
            Rect(449, 179, 312, 39, PaperColor);
            Text(TEXT("To: No contacts available"), 461, 188, 14, Ink, Regular);
            Text(TEXT("Find a contact during an interview\nor investigation before writing."),
                449, 240, 14, Ink, Regular);
            break;
        default:
            break;
        }
        return;
    }

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(IntroMail); ++Index)
    {
        if (Index == ToyOwnerMail && !HasToyOwnerMail()) { continue; }
        const double Y = 102 + Index * 76;
        Rect(144, Y, 279, 75, SelectedMail == Index ? Selected : PaperColor);
        Rect(144, Y + 75, 279, 1, Rule);
        if (Index == ToyOwnerMail ? !bToyOwnerMailRead : !bMailRead[Index])
        {
            DrawMailUnreadDot(At(157, Y + 25), 4.8 * S);
        }
        Text(IntroMail[Index].Subject, 169, Y + 12, 14, Ink, Bold, 239);
        Text(IntroMail[Index].Sender, 169, Y + 40, 11.5f, Ink, Regular);
        if (Index == WelcomeMail && !bSponsorshipClaimed)
        {
            Rect(336, Y + 40, 78, 23, LightPaper);
            Text(TEXT("Unclaimed"), 343, Y + 43, 10, Ink, Bold);
        }
        MailActions.Emplace(FBox2D(At(144, Y), At(423, Y + 75)),
            Index == LeaseMail ? EMailAction::OpenLease
                : Index == WelcomeMail ? EMailAction::OpenWelcome : EMailAction::OpenToyOwner);
    }

    if (HasLostOwnerMail())
    {
        constexpr double Y = 330;
        Rect(144, Y, 279, 75, SelectedMail == LostOwnerMail ? Selected : PaperColor);
        Rect(144, Y + 75, 279, 1, Rule);
        if (!LostCases.Last().bMailRead) { DrawMailUnreadDot(At(157, Y + 25), 4.8 * S); }
        Text(LostOwnerMailSubject(), 169, Y + 12, 14, Ink, Bold, 239);
        Text(LostOwnerMailSender(), 169, Y + 40, 11.5f, Ink, Regular, 239);
        MailActions.Emplace(FBox2D(At(144, Y), At(423, Y + 75)), EMailAction::OpenLostOwner);
    }
    if (SelectedMail == LostOwnerMail)
    {
        if (HasLostOwnerMail())
        {
            Text(LostOwnerMailSubject(), 445, 126, 20, Ink, Bold, 318);
            Text(FString::Printf(TEXT("From: %s"), *LostOwnerMailSender()), 445, 167, 13, Ink, Regular, 318);
            Rect(445, 202, 321, 1, Rule);
            Text(LostOwnerMailBody(), 445, 224, 14, Ink, Regular, 318);
            return;
        }
        SelectedMail = INDEX_NONE;
    }

    if (SelectedMail == INDEX_NONE)
    {
        Text(TEXT("Select a message"), 448, 131, 20, Ink, Bold);
        return;
    }
    const FIntroMail& Message = IntroMail[SelectedMail];
    Text(Message.Subject, 445, 126, 20, Ink, Bold, 318);
    Text(FString::Printf(TEXT("From: %s"), Message.Sender), 445, 167, 13, Ink, Regular);
    Rect(445, 202, 321, 1, Rule);
    if (SelectedMail == ToyOwnerMail)
    {
        Text(bToyReturned ? TEXT("Jamie has the dinosaur back.\nThank you for helping us!")
            : TEXT("Hi! Jamie lost a little green dinosaur.\nIt has a scratch on the tail.\n\n"
                "We'll meet you by the park notice board.\nIf you left it at the fountain, please\nshow us where you saw it."),
            445, 224, 14, Ink, Regular);
        return;
    }
    if (SelectedMail == LeaseMail)
    {
        Text(TEXT("Your lease for the newsroom is active.\n\n"
                  "Rent is $100 every seven days.\nWe'll debit your balance at midnight.\n"
                  "Your first payment is due on Day 7.\n\n"
                  "Good luck with your first edition."),
            445, 224, 14, Ink, Regular);
        return;
    }

    Text(TEXT("Welcome to Rivermere, reporter.\nYour special edition goes out after Day 30.\n\n"
              "To help you get the newsroom off the\nground, we've attached $60 in\nstartup sponsorship.") ,
        445, 224, 14, Ink, Regular);

    Rect(445, 411, 321, 121, Rule);
    Rect(446, 412, 319, 119, PaperColor);
    if (DesktopIcons.IsValidIndex(0) && DesktopIcons[0])
    {
        UTexture2D* Icon = DesktopIcons[0];
        const double Fit = 64 * S / FMath::Max(Icon->GetSizeX(), Icon->GetSizeY());
        const FVector2D Extent = FVector2D(Icon->GetSizeX(), Icon->GetSizeY()) * Fit;
        const FVector2D IconAt = At(493, 471) - Extent * 0.5;
        DrawTexture(Icon, IconAt.X, IconAt.Y, Extent.X, Extent.Y,
            0, 0, 1, 1, FLinearColor::White, BLEND_Translucent);
    }
    Text(TEXT("Startup Sponsorship"), 540, 426, 14, Ink, Bold);
    Text(FString::Printf(TEXT("$%d"), Sponsorship), 540, 453, 14, Ink, Bold);
    Button(EMailAction::Claim, bSponsorshipClaimed ? FString(TEXT("Claimed")) :
        FString::Printf(TEXT("Claim $%d"), Sponsorship), 540, 489, 184,
        !bSponsorshipClaimed, !bSponsorshipClaimed);
}
