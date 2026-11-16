#include "ReporterGameMode.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"

namespace
{
constexpr int32 SpecialIssueDay = 30;
constexpr int32 ExhaustionLimit = 3;
const FLinearColor Navy(0.009f, 0.039f, 0.054f);
const FLinearColor Ink(0.025f, 0.071f, 0.085f);
const FLinearColor MutedInk(0.16f, 0.23f, 0.23f);
const FLinearColor PaperEdge(0.93f, 0.83f, 0.64f);
const FLinearColor LightPaper(0.99f, 0.92f, 0.77f);
const FLinearColor Gold(0.94f, 0.54f, 0.15f);
const FLinearColor Rule(0.56f, 0.43f, 0.29f);

struct FEndingStory
{
    const TCHAR* Art;
    const TCHAR* Section;
    const TCHAR* Headline;
    const TCHAR* Body;
};

const FEndingStory Stories[] = {
    { TEXT("career-master-interviewer"), TEXT("THE REPORTER"), TEXT("People Tell You Things"),
        TEXT("You know when to ask the next question and when to leave a little silence. People tell you things they hadn't planned to say. You listen long enough to understand why.") },
    { TEXT("career-master-investigator"), TEXT("THE REPORTER"), TEXT("A Second Look"),
        TEXT("You spot the detail that sends you back to the scene. A photograph, a witness, a mark on the pavement. You know how to fit them together and where the gaps still are.") },
    { TEXT("career-master-editor"), TEXT("THE REPORTER"), TEXT("The Right Words"),
        TEXT("You can tell which photograph belongs on the front page and which sentence needs another pass. The finished edition feels effortless. You know exactly how much work that took.") },
    { TEXT("career-developing"), TEXT("THE REPORTER"), TEXT("Finding Your Eye"),
        TEXT("The work is starting to feel familiar. Some days you know where to look as soon as you arrive. On the others, you bring the notes home and give them another read.") },
    { TEXT("career-novice"), TEXT("THE REPORTER"), TEXT("Room in the Margins"),
        TEXT("There are still plenty of pencil marks on your drafts. Every time you go out with the camera, you come back with a better idea of what to look for next time.") },
    { TEXT("ethics-high"), TEXT("THE RECORD"), TEXT("A Clear Conscience"),
        TEXT("You made time for people whose troubles could easily have passed unnoticed. The work you can stand behind is still there after the headlines have faded.") },
    { TEXT("ethics-middle"), TEXT("THE RECORD"), TEXT("The Choices Between"),
        TEXT("Some choices came easily. Others sent you back to your notebook to work out what you owed the people in the story. There will be more chances to get that right.") },
    { TEXT("ethics-low"), TEXT("THE RECORD"), TEXT("What the Headline Cost"),
        TEXT("A headline can go to press in minutes. Its consequences can stay with the people in it for weeks. You have some thinking to do before the next edition.") },
    { TEXT("reputation-high"), TEXT("AROUND TOWN"), TEXT("A Knock at the Door"),
        TEXT("Word gets around that you can be trusted with a story. A neighbor who used to pass your door now stops to tell you what happened down the street.") },
    { TEXT("reputation-middle"), TEXT("AROUND TOWN"), TEXT("A Nod on the Street"),
        TEXT("A few people recognize you on your rounds. They nod over their coffee and turn the page. A familiar face is a good place to start.") },
    { TEXT("reputation-low"), TEXT("AROUND TOWN"), TEXT("Behind the Curtains"),
        TEXT("Some neighbors have grown wary of what ends up in print. You notice the pause when you introduce yourself. It will take time and patient reporting to earn another chance.") },
    { TEXT("business-strong"), TEXT("THE LEDGER"), TEXT("Ready for Another Run"),
        TEXT("The next rent payment is covered and there is money to put back into the paper. For the first time, you can sit down and plan an issue a little further ahead.") },
    { TEXT("business-stable"), TEXT("THE LEDGER"), TEXT("Just Enough in the Till"),
        TEXT("You count out the next rent payment and put it aside. The paper can keep going. Every story that earns its place in the next issue will make that a little easier.") },
    { TEXT("business-strained"), TEXT("THE LEDGER"), TEXT("An Outstanding Balance"),
        TEXT("The office is still open, and the next rent payment is already on your mind. You turn the ledger to a clean page. The next few editions will matter.") },
    { TEXT("failure-hospital"), TEXT("HEALTH LEAVE"), TEXT("Publication Suspended"),
        TEXT("After one too many days on empty, you have to spend some time in hospital. Your notebook is still on the bedside table. Let yourself rest. It will be there when you are ready.") },
    { TEXT("failure-bankruptcy"), TEXT("CLOSING NOTICE"), TEXT("The Last Edition"),
        TEXT("Rent came due and the paper couldn't cover it. You close the ledger with a balance still owed, turn off the newsroom light and take the last edition home. There are other ways to begin again.") }
};

enum EStoryIndex
{
    Interviewing, Investigation, Editing, Developing, Novice,
    EthicsHigh, EthicsMiddle, EthicsLow,
    ReputationHigh, ReputationMiddle, ReputationLow,
    BusinessStrong, BusinessStable, BusinessStrained,
    Hospital, Bankruptcy
};
static_assert(Bankruptcy + 1 == UE_ARRAY_COUNT(Stories));
}

int32 AReporterHUD::GetReporterReputation() const
{
    int32 Reputation = 50;
    for (const FPublishedArticle& Article : PublishedArticles)
    {
        if (Article.Angle == 0) { Reputation += 4; }
        if (Article.Angle == 1) { Reputation += 3; }
        if (Article.Angle == 2) { Reputation -= Article.bExposed ? 12 : 2; }
    }
    return FMath::Clamp(Reputation, 0, 100);
}

bool AReporterHUD::SettleEndingDay(int32 ClosedDay)
{
    if (ClosedDay % RentInterval == 0)
    {
        if (ReporterCash < RentAmount)
        {
            CreateEndingIssue(EEndingKind::Bankruptcy, ClosedDay);
            return true;
        }
        ReporterCash -= RentAmount;
    }

    ExhaustedNights = ReporterStamina <= ExhaustedStamina ? FMath::Min(ExhaustionLimit, ExhaustedNights + 1) : 0;
    if (ExhaustedNights > 0)
    {
        ReporterHealth = FMath::Max(0, ReporterHealth - ExhaustionHealthLoss);
    }
    if (ReporterHealth == 0)
    {
        CreateEndingIssue(EEndingKind::Hospital, ClosedDay);
        return true;
    }
    if (ClosedDay == SpecialIssueDay && EndingIssue.Kind == EEndingKind::None)
    {
        CreateEndingIssue(EEndingKind::Special, ClosedDay);
        return true;
    }
    return false;
}

void AReporterHUD::CreateEndingIssue(EEndingKind Kind, int32 Day)
{
    EndingIssue = {};
    EndingIssue.Kind = Kind;
    EndingIssue.Day = Day;
    if (Kind == EEndingKind::Special)
    {
        int32 Best = Interviewing;
        for (int32 Skill = Investigation; Skill <= Editing; ++Skill)
        {
            if (ReporterSkills[Skill] >= ReporterSkills[Best]) { Best = Skill; }
        }
        const int32 Skill = ReporterSkills[Best];
        const int32 Reputation = GetReporterReputation();
        int32 Exposed = 0;
        int32 Revenue = 0;
        for (const FPublishedArticle& Article : PublishedArticles)
        {
            Exposed += Article.bExposed ? 1 : 0;
            Revenue += Article.Earnings;
        }
        const int32 Ethics = ReporterMorality - 3 * Exposed;
        EndingIssue.Stories = {
            Skill >= 60 ? Best : Skill >= 30 ? Developing : Novice,
            Ethics >= 5 ? EthicsHigh : Ethics < 0 ? EthicsLow : EthicsMiddle,
            Reputation >= 65 ? ReputationHigh : Reputation <= 40 ? ReputationLow : ReputationMiddle,
            ReporterCash >= 2 * RentAmount ? BusinessStrong : ReporterCash >= RentAmount ? BusinessStable : BusinessStrained
        };
        const TCHAR* Skills[] = {TEXT("Interviewing"), TEXT("Investigation"), TEXT("Editing")};
        EndingIssue.Captions.Add(FString::Printf(TEXT("%s %d/100  |  Stories published %d"),
            Skills[Best], Skill, PublishedArticles.Num()));
        int32 Returned = bToyReturned ? 1 : 0;
        for (const FLostCase& Case : LostCases) { Returned += Case.bReturned ? 1 : 0; }
        bool bRepairedTile = false;
        for (int32 RepairDay : FallRepairDays)
        {
            bRepairedTile |= RepairDay > 0 && RepairDay <= Day;
        }
        if (Exposed > 0)
        {
            EndingIssue.Captions.Add(FString::Printf(TEXT("Exaggerated stories exposed  %d"), Exposed));
        }
        else if (bRepairedTile)
        {
            EndingIssue.Captions.Add(TEXT("A loose paving stone was repaired after your report."));
        }
        else
        {
            EndingIssue.Captions.Add(FString::Printf(TEXT("Lost items returned %d  |  Morality %+d"), Returned, ReporterMorality));
        }
        EndingIssue.Captions.Add(FString::Printf(TEXT("Reputation %d/100  |  Followers %d"), Reputation, NewsroomFollowers));
        EndingIssue.Captions.Add(FString::Printf(TEXT("Article revenue $%d  |  Cash on hand $%d"), Revenue, ReporterCash));
    }
    else if (Kind == EEndingKind::Bankruptcy)
    {
        EndingIssue.Stories.Add(Bankruptcy);
        EndingIssue.Captions.Add(FString::Printf(TEXT("Rent due $%d  |  Cash $%d  |  Shortfall $%d"),
            RentAmount, ReporterCash, RentAmount - ReporterCash));
    }
    else
    {
        EndingIssue.Stories.Add(Hospital);
        EndingIssue.Captions.Add(FString::Printf(TEXT("Health  %d / %d"), ReporterHealth, MaxHealth));
    }
    OpenEnding();
}

void AReporterHUD::OpenEnding(bool bArchive)
{
    bEndingOpen = true;
    bReadingArchivedEnding = bArchive;
    EndingPage = 0;
    EndingPrevious = EndingNext = EndingClose = FBox2D(ForceInit);
    HideStoryInputs();
    bCameraMode = false;
    if (GetOwningPlayerController())
    {
        PlayActivityMusic(EndingIssue.Kind == EEndingKind::Special ? SpecialEndingMusic.Get() : FailedEndingMusic.Get());
    }
}

void AReporterHUD::DrawEnding()
{
    if (!EndingIssue.Stories.IsValidIndex(EndingPage)) { return; }
    const FEndingStory& Story = Stories[EndingIssue.Stories[EndingPage]];
    const bool bSpecial = EndingIssue.Kind == EEndingKind::Special;
    const FString File = FString::Printf(TEXT("%s-%s.jpg"), Story.Art,
        bFemaleReporter ? TEXT("female") : TEXT("male"));
    if (LoadedEndingArt != File)
    {
        LoadedEndingArt = File;
        EndingTexture = FImageUtils::ImportFileAsTexture2D(
            FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt/Endings") / File);
        if (!EndingTexture) { UE_LOG(LogTemp, Error, TEXT("Ending artwork missing: %s"), *File); }
    }

    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    constexpr double Width = 1160;
    constexpr double Height = 628;
    const double S = FMath::Min(View.X / (Width + 48), View.Y / (Height + 40));
    const FVector2D Origin = (View - FVector2D(Width, Height) * S) * 0.5;
    const auto At = [&](double X, double Y) { return Origin + FVector2D(X, Y) * S; };
    const auto Rect = [&](double X, double Y, double W, double H, FLinearColor Color)
    {
        const FVector2D P = At(X, Y);
        DrawRect(Color, P.X, P.Y, W * S, H * S);
    };
    UFont* Medium = GEngine->GetMediumFont();
    UFont* Serif = TitleBoldFont ? TitleBoldFont.Get() : Medium;
    const auto Text = [&](const FString& Value, double X, double Y, float Size, UFont* Font,
        FLinearColor Color, double MaxWidth = 0)
    {
        float Scale = Size * S / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size);
        if (MaxWidth > 0)
        {
            float W, H;
            MeasureUiText(Value, W, H, Font, Scale);
            Scale *= FMath::Min(1.0, MaxWidth * S / FMath::Max(1.0f, W));
        }
        const FVector2D P = At(X, Y);
        DrawUiText(Value, Color, P.X, P.Y, Font, Scale);
    };
    const auto Button = [&](const TCHAR* Label, double X, double W, bool bPrimary)
    {
        Rect(X, 567, W, 36, bPrimary ? Ink : Rule);
        if (!bPrimary) { Rect(X + 1, 568, W - 2, 34, LightPaper); }
        Text(Label, X + 12, 574, 17, Medium, bPrimary ? LightPaper : Ink, W - 24);
        return FBox2D(At(X, 567), At(X + W, 603));
    };

    DrawRect(Navy, 0, 0, View.X, View.Y);
    Rect(0, 0, Width, Height, PaperEdge);
    Rect(8, 8, Width - 16, Height - 16, LightPaper);
    Rect(32, 25, 1096, 3, Ink);
    Text(TEXT("RIVERMERE NEWS"), 33, 35, 53, Serif, Ink);
    Text(bSpecial ? TEXT("THIRTIETH-DAY SPECIAL EDITION") : TEXT("EXTRA EDITION"), 35, 106, 17, Medium, MutedInk);
    Text(FString::Printf(TEXT("DAY %02d"), EndingIssue.Day), 1023, 105, 19, Serif, Ink, 105);
    Rect(32, 136, 1096, 2, Ink);
    Rect(32, 141, 1096, 1, Rule);

    Rect(32, 161, 668, 376, Navy);
    if (EndingTexture)
    {
        const FVector2D P = At(34, 163);
        DrawTexture(EndingTexture, P.X, P.Y, 664 * S, 373.5 * S, 0, 0, 1, 1,
            FLinearColor::White, BLEND_Opaque);
    }
    Rect(721, 162, 1, 374, Rule);
    Text(Story.Section, 744, 168, 17, Medium, MutedInk);
    Text(Story.Headline, 744, 202, 33, Serif, Ink, 382);
    Rect(744, 254, 58, 4, Gold);
    DrawStoryParagraphs(Story.Body, At(744, 276), 378 * S,
        17.0f * S / FMath::Max(1.0f, Medium->GetLegacySlateFontInfo().Size), 7);
    Rect(744, 502, 381, 1, Rule);
    if (EndingIssue.Captions.IsValidIndex(EndingPage))
    {
        Text(EndingIssue.Captions[EndingPage], 744, 511, 15, Medium, MutedInk, 381);
    }
    Rect(32, 550, 1096, 1, Rule);

    EndingPrevious = EndingNext = EndingClose = FBox2D(ForceInit);
    if (bSpecial)
    {
        if (EndingPage > 0) { EndingPrevious = Button(TEXT("Previous"), 32, 114, false); }
        Text(FString::Printf(TEXT("%d / %d"), EndingPage + 1, EndingIssue.Stories.Num()),
            163, 574, 17, Medium, MutedInk);
        if (EndingPage + 1 < EndingIssue.Stories.Num()) { EndingNext = Button(TEXT("Next page"), 1006, 122, true); }
        if (bReadingArchivedEnding) { EndingClose = Button(TEXT("Back to Newsroom"), 749, 194, false); }
        else if (!EndingNext.bIsValid) { EndingClose = Button(TEXT("Continue Publishing"), 916, 212, true); }
    }
    else
    {
        EndingClose = Button(TEXT("Return to Title"), 951, 177, true);
    }
}

void AReporterHUD::ClickEnding(bool bUsePointer)
{
    if (bUsePointer)
    {
        float X, Y;
        if (!GetOwningPlayerController()->GetMousePosition(X, Y)) { return; }
        const FVector2D Pointer(X, Y);
        if (EndingPrevious.bIsValid && EndingPrevious.IsInsideOrOn(Pointer))
        {
            --EndingPage;
            return;
        }
        if (!EndingNext.bIsValid || !EndingNext.IsInsideOrOn(Pointer))
        {
            if (!EndingClose.bIsValid || !EndingClose.IsInsideOrOn(Pointer)) { return; }
        }
        else
        {
            ++EndingPage;
            return;
        }
    }
    else if (EndingPage + 1 < EndingIssue.Stories.Num())
    {
        ++EndingPage;
        return;
    }

    if (EndingIssue.Kind != EEndingKind::Special)
    {
        UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
        return;
    }
    bEndingOpen = false;
    EndingTexture = nullptr;
    LoadedEndingArt.Reset();
    StopActivityMusic();
    bReadingArchivedEnding = false;
}

void AReporterHUD::DrawEndingWarnings()
{
    const bool bRentDue = GetGameDay() % RentInterval == 0 && ReporterCash < RentAmount;
    if (!bRentDue && ExhaustedNights == 0) { return; }
    const double S = GetHudScale();
    const double W = 380 * S;
    const double X = (Canvas->ClipX - W) * 0.5;
    double Y = 92 * S;
    const auto Notice = [&](const FString& Message)
    {
        DrawRect(Ink, X, Y, W, 25 * S);
        DrawUiText(Message, LightPaper, X + 10 * S, Y + 3 * S, GEngine->GetMediumFont(), 0.85 * S);
        Y += 29 * S;
    };
    if (bRentDue) { Notice(TEXT("Rent: $100 due at midnight. Check your balance.")); }
    if (ExhaustedNights > 0)
    {
        Notice(FString::Printf(TEXT("Health %d/100. Rest before midnight."), ReporterHealth));
    }
}

void AReporterHUD::PreviewEndingFromCommandLine()
{
#if !UE_BUILD_SHIPPING
    static bool bPreviewConsumed = false;
    if (bPreviewConsumed) { return; }
    FString Preview;
    if (!FParse::Value(FCommandLine::Get(), TEXT("ReporterEndingPreview="), Preview)) { return; }
    const bool bSpecial = Preview.Equals(TEXT("Special"), ESearchCase::IgnoreCase);
    const bool bHospital = Preview.Equals(TEXT("Hospital"), ESearchCase::IgnoreCase);
    if (!bSpecial && !bHospital && !Preview.Equals(TEXT("Bankruptcy"), ESearchCase::IgnoreCase)) { return; }
    bPreviewConsumed = true;
    BeginNewsroomPlay();
    bFemaleReporter = FParse::Param(FCommandLine::Get(), TEXT("ReporterEndingFemale"));
    if (bSpecial)
    {
        ReporterSkills[Editing] = 66;
        ReporterMorality = 5;
        NewsroomFollowers = 18;
        ReporterCash = 120;
        OutdoorGameMinutes = SpecialIssueDay * 1440.0;
        CreateEndingIssue(EEndingKind::Special, SpecialIssueDay);
    }
    else
    {
        ExhaustedNights = bHospital ? ExhaustionLimit : 0;
        ReporterHealth = bHospital ? 0 : MaxHealth;
        ReporterCash = 42;
        const int32 Day = bHospital ? 5 : RentInterval;
        OutdoorGameMinutes = Day * 1440.0;
        CreateEndingIssue(bHospital ? EEndingKind::Hospital : EEndingKind::Bankruptcy, Day);
    }
#endif
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReporterEndingTest, "Reporter.Endings.Settlement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReporterEndingTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Ending test world"), World)) { return false; }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    using Kind = AReporterHUD::EEndingKind;
    AReporterHUD* Hud = World->SpawnActor<AReporterHUD>();
    if (!TestNotNull(TEXT("Ending test HUD"), Hud)) { return false; }
    TestTrue(TEXT("The special edition loads the looping Four-Note Motif"),
        Hud->SpecialEndingMusic && Hud->SpecialEndingMusic->GetFName() == FName(TEXT("FourNoteMotif")) &&
        Hud->SpecialEndingMusic->IsLooping());
    TestTrue(TEXT("The closing notices load the one-shot Final Major Sixth"),
        Hud->FailedEndingMusic && Hud->FailedEndingMusic->GetFName() == FName(TEXT("FinalMajorSixth")) &&
        !Hud->FailedEndingMusic->IsLooping());

    Hud->OutdoorGameMinutes = 7 * 1440 - 1;
    Hud->ReporterCash = 100;
    Hud->AdvanceGameMinutes(2);
    TestEqual(TEXT("First rent debited once when Day 7 closes"), Hud->ReporterCash, 0);
    TestFalse(TEXT("Exact rent balance keeps the paper open"), Hud->bEndingOpen);
    Hud->AdvanceGameMinutes(1);
    TestEqual(TEXT("Same midnight is not charged again"), Hud->ReporterCash, 0);

    Hud->OutdoorGameMinutes = 14 * 1440 - 1;
    Hud->ReporterCash = 42;
    Hud->AdvanceGameMinutes(12);
    TestTrue(TEXT("Missed rent opens the single-page bankruptcy extra"), Hud->bEndingOpen &&
        Hud->EndingIssue.Kind == Kind::Bankruptcy && Hud->EndingIssue.Stories == TArray<int32>{Bankruptcy});
    TestEqual(TEXT("Bankruptcy is dated to the day that closed"), Hud->EndingIssue.Day, 14);
    TestEqual(TEXT("Unpaid rent leaves a shortfall instead of debiting an imaginary balance"), Hud->ReporterCash, 42);
    const double FailedAt = Hud->OutdoorGameMinutes;
    Hud->AdvanceGameMinutes(40);
    TestEqual(TEXT("Time is held while the extra is open"), Hud->OutdoorGameMinutes, FailedAt);

    Hud->EndingIssue = {};
    Hud->bEndingOpen = false;
    Hud->OutdoorGameMinutes = 1440 - 1;
    Hud->ReporterStamina = 10;
    Hud->AdvanceGameMinutes(1440 + 1);
    TestEqual(TEXT("Two exhausted nights remain recoverable"), Hud->ExhaustedNights, 2);
    TestFalse(TEXT("Two exhausted nights do not end the run"), Hud->bEndingOpen);
    Hud->ReporterStamina = 70;
    Hud->AdvanceGameMinutes(1440);
    TestEqual(TEXT("Rest breaks the consecutive exhaustion count"), Hud->ExhaustedNights, 0);
    TestEqual(TEXT("Rest protects health without healing previous losses"), Hud->ReporterHealth, 30);
    Hud->bInHospital = true;
    Hud->ReporterCash = 49;
    TestTrue(TEXT("Treatment requires the full price"), !Hud->TreatHealth() && Hud->ReporterHealth == 30);
    Hud->ReporterCash = 100;
    TestTrue(TEXT("Treatment restores exactly 30 health for $50"), Hud->TreatHealth() &&
        Hud->ReporterHealth == 60 && Hud->ReporterCash == 50);
    Hud->ReporterHealth = 95;
    TestTrue(TEXT("Treatment caps health at 100"), Hud->TreatHealth() && Hud->ReporterHealth == 100);
    Hud->bInHospital = false;
    Hud->ReporterStamina = 0;
    Hud->AdvanceGameMinutes(3 * 1440);
    TestTrue(TEXT("Three exhausted nights open a single-page hospital extra"), Hud->bEndingOpen &&
        Hud->EndingIssue.Kind == Kind::Hospital && Hud->EndingIssue.Stories == TArray<int32>{Hospital});

    Hud->EndingIssue = {};
    Hud->bEndingOpen = false;
    Hud->ReporterHealth = AReporterHUD::MaxHealth;
    Hud->OutdoorGameMinutes = 30 * 1440 - 1;
    Hud->ReporterStamina = 70;
    Hud->ReporterSkills[Editing] = 65;
    Hud->ReporterMorality = 7;
    Hud->ReporterCash = 180;
    Hud->AdvanceGameMinutes(2);
    TestTrue(TEXT("Day 30 assembles independently tiered sections"), Hud->bEndingOpen &&
        Hud->EndingIssue.Kind == Kind::Special &&
        Hud->EndingIssue.Stories == TArray<int32>{Editing, EthicsHigh, ReputationMiddle, BusinessStable});
    for (int32 Page = 0; Page < 4; ++Page) { Hud->ClickEnding(false); }
    TestFalse(TEXT("Closing the special returns to play"), Hud->bEndingOpen);
    const TArray<FString> Captions = Hud->EndingIssue.Captions;
    Hud->ReporterCash = 400;
    Hud->AdvanceGameMinutes(1440);
    TestTrue(TEXT("The archived edition keeps its original captions"), Hud->EndingIssue.Captions == Captions);

    Hud->bInEditorial = Hud->bHasBed = true;
    Hud->BedBounds = FBox2D(FVector2D::ZeroVector, FVector2D::UnitVector);
    Hud->EditorialPosition = Hud->BedPosition - FVector2D(0, Hud->GetBedRadius().Y);
    Hud->EndingIssue = {};
    Hud->OutdoorGameMinutes = 6 * 1440 + 22 * 60;
    Hud->ReporterCash = 100;
    Hud->ReporterStamina = 4;
    auto& ToyArticle = Hud->PublishedArticles.AddDefaulted_GetRef();
    ToyArticle.StoryType = 1;
    ToyArticle.Angle = 2;
    ToyArticle.Day = 6;
    ToyArticle.InitialViews = 120;
    Hud->SeedReaderComments(ToyArticle, 0);
    TestTrue(TEXT("Fountain story has both a topic-specific and general comment"),
        ToyArticle.Comments.Num() == 2 && ToyArticle.Comments[0].Body.Contains(TEXT("toy dinosaur")) &&
        ToyArticle.Comments[0].Reader != ToyArticle.Comments[1].Reader);
    Hud->bToyReturned = true;
    Hud->bConfirmMorningRest = true;
    Hud->ClickMorningRestConfirmation(false);
    TestTrue(TEXT("Morning rest crosses one midnight, restores stamina, charges rent, and settles traffic"),
        !Hud->bConfirmMorningRest && Hud->GetGameDay() == 8 && Hud->OutdoorGameMinutes == 7 * 1440 + 8 * 60 &&
        Hud->ReporterStamina == 100 && Hud->ReporterCash == ToyArticle.Earnings && ToyArticle.Earnings > 0);
    TestTrue(TEXT("Returned toy adds a dated follow-up once"),
        ToyArticle.bPrintedEventFollowup && ToyArticle.Comments.Last().Day == 8 &&
        ToyArticle.Comments.Last().Body.Contains(TEXT("made it home")));
    const int32 CommentCount = ToyArticle.Comments.Num();
    Hud->AdvanceGameMinutes(1440);
    TestEqual(TEXT("Overnight follow-up is not repeated"), ToyArticle.Comments.Num(), CommentCount);
    Hud->OutdoorGameMinutes = 30 * 1440 - 5;
    Hud->RestAtEditorialBed(true);
    TestTrue(TEXT("Morning rest still opens the Day 30 issue and resumes on the next morning"),
        Hud->bEndingOpen && Hud->EndingIssue.Kind == Kind::Special &&
        Hud->OutdoorGameMinutes == 30 * 1440 + 8 * 60);

    Hud->OutdoorGameMinutes = 4 * 1440 - 1;
    TestFalse(TEXT("Bird does not appear before Day 5"), Hud->IsBirdSceneActive());
    Hud->OutdoorGameMinutes = 4 * 1440;
    TestTrue(TEXT("First bird appears on Day 5"), Hud->IsBirdSceneActive());
    Hud->ApplyBirdTemplate();
    TestEqual(TEXT("Initial report is unverified"), Hud->StoryAngle, 0);
    Hud->PublishBirdStory();
    TestTrue(TEXT("Unverified report schedules a new scene three days later"),
        Hud->BirdReports == 1 && Hud->BirdNextDay == 8 && !Hud->IsBirdSceneActive());
    Hud->OutdoorGameMinutes = 7 * 1440;
    Hud->StoryKind = 4;
    Hud->ApplyBirdTemplate();
    TestEqual(TEXT("Follow-up identifies glass"), Hud->StoryAngle, 1);
    Hud->EditorPhotoFiles = {TEXT("Photo_old_Bird1.png"), TEXT("Photo_new_Bird2.png")};
    Hud->ArticlePhotos = {0};
    TestFalse(TEXT("First photo cannot stand in for follow-up evidence"), Hud->CanPublishBirdStory());
    Hud->ArticlePhotos = {1};
    TestTrue(TEXT("Fresh follow-up photograph qualifies"), Hud->CanPublishBirdStory());
    Hud->PublishBirdStory();
    TestTrue(TEXT("Verified story schedules replacement the next day"),
        Hud->BirdFixDay == 9 && !Hud->IsBirdSceneActive());
    Hud->OutdoorGameMinutes = 4 * 1440;
    Hud->BirdReports = Hud->BirdFixDay = 0;
    Hud->BirdNextDay = 5;
    Hud->bBirdGlassKnown = true;
    Hud->ApplyBirdTemplate();
    Hud->PublishBirdStory();
    TestTrue(TEXT("Recognizing the glass on the first visit skips the second incident"),
        Hud->BirdReports == 1 && Hud->BirdFixDay == 6 && !Hud->IsBirdSceneActive());

    const FString Art = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt/Endings");
    for (const FEndingStory& Story : Stories)
    {
        for (const TCHAR* Gender : {TEXT("male"), TEXT("female")})
        {
            const FString File = FString::Printf(TEXT("%s-%s.jpg"), Story.Art, Gender);
            TestTrue(File, FPaths::FileExists(Art / File));
        }
    }
    return true;
}
#endif
