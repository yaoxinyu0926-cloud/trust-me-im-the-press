#include "ReporterGameMode.h"

#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"

namespace
{
const FLinearColor Ink(0.16f, 0.10f, 0.07f);
const FLinearColor Cream(0.97f, 0.89f, 0.73f);
const FLinearColor Teal(0.14f, 0.35f, 0.32f);
const FLinearColor Coral(0.66f, 0.29f, 0.19f);
const FLinearColor Line(0.62f, 0.52f, 0.39f);
const FLinearColor Pale(0.99f, 0.94f, 0.84f);
enum ENewsroomAction { ShowStory = -9, ShowComments, SpecialEdition, Overview, Articles, Older, Newer, PreviousPage, NextPage };
constexpr int32 EntriesPerPage = 3;
constexpr int32 RevenueDays = 4;
constexpr int32 ViewsPerDollar = 30;
constexpr int32 FreeStoryBaseViews = 90;
constexpr int32 FountainBaseViews = 200;
constexpr int32 ViewsPerFollower = 2;
constexpr float AngleMultipliers[] = {1.0f, 1.2f, 1.6f};
constexpr float ExposureChance = 0.5f;
constexpr int32 ExposureFollowerLoss = 8;
struct FReaderLine
{
    const TCHAR* Reader;
    const TCHAR* Body;
};
const FReaderLine CommonComments[] = {
    { TEXT("Nora P."), TEXT("Nice to see a story about this end of town for once.") },
    { TEXT("Milo R."), TEXT("I always check the photo before I read the headline.") },
    { TEXT("Ruth at No. 8"), TEXT("I read this with my morning coffee. Keep the local stories coming.") },
    { TEXT("Cal V."), TEXT("I'd like to hear what someone who was there has to say.") },
    { TEXT("Bea L."), TEXT("I walk past these streets every day. Funny what you only notice when somebody points it out.") }
};
// Rows follow the event picker; columns follow the editor's three angles.
const FReaderLine EventComments[][3] = {
    {
        { TEXT("Nora P."), TEXT("My kid has the same one. I hope whoever lost it sees this.") },
        { TEXT("Milo R."), TEXT("Can we see the whole photo? It's hard to judge the size from a close-up.") },
        { TEXT("Milo R."), TEXT("Come on. That's a toy dinosaur. You can see the seam down its back.") }
    },
    {
        { TEXT("Ruth at No. 8"), TEXT("I hope they're all right. I use that pavement every morning.") },
        { TEXT("Cal V."), TEXT("I'm glad somebody went back to check what actually happened.") },
        { TEXT("Bea L."), TEXT("That headline sounds awfully sure of itself. Did you talk to the person who fell?") }
    },
    {
        { TEXT("Ruth at No. 8"), TEXT("Losing a wallet is miserable. I hope it got back to its owner.") },
        { TEXT("Cal V."), TEXT("Taking it to the front desk is exactly what I'd do. Glad you checked.") },
        { TEXT("Nora P."), TEXT("Picking up a wallet isn't proof you meant to keep it.") }
    },
    {
        { TEXT("Nora P."), TEXT("I hope it pulled through. Has anyone seen this happen before?") },
        { TEXT("Cal V."), TEXT("I never noticed how clearly those windows mirror the trees.") },
        { TEXT("Milo R."), TEXT("A clear picture helps when you're asking the mall to take a look.") }
    }
};
const FReaderLine ExposureComment = { TEXT("Bea L."),
    TEXT("I shared this before the facts came out. Could you put a correction where everyone can see it?") };
}

AReporterHUD::FNewspaper AReporterHUD::GetDraftNewspaper() const
{
    FNewspaper Page;
    Page.Headline = DraftHeadline;
    Page.Body = DraftBody;
    Page.Photos = ArticlePhotos;
    for (int32 Index : ArticlePhotos)
    {
        Page.Edits.Add(DraftPhotoEdits[Index]);
    }
    Page.Layout = ArticleLayout;
    Page.TitleSize = HeadlineSize;
    Page.BodySize = BodySize;
    Page.Paper = Paper;
    Page.Stamp = Stamp;
    Page.bBold = bHeadlineBold;
    Page.bUnderline = bUnderline;
    return Page;
}

FBox2D AReporterHUD::DrawStoryPhoto(int32 Index, const FEditorPhoto& Edit, FVector2D Origin,
    FVector2D Size, bool bTopAligned)
{
    if (!EditorPhotos.IsValidIndex(Index) || !EditorPhotos[Index])
    {
        return FBox2D(Origin, Origin);
    }
    UTexture2D* Texture = EditorPhotos[Index];
    const FVector4& Region = Edit.Crop;
    const bool bSideways = Edit.Turn % 2 != 0;
    const double Aspect = double(Texture->GetSizeX()) * (Region.Z - Region.X) /
        (Texture->GetSizeY() * (Region.W - Region.Y));
    const double H = FMath::Min(bSideways ? Size.X : Size.Y, (bSideways ? Size.Y : Size.X) / Aspect);
    const double W = H * Aspect;
    const FVector2D Visible = bSideways ? FVector2D(H, W) : FVector2D(W, H);
    const FVector2D Center = Origin + FVector2D(Size.X, bTopAligned ? Visible.Y : Size.Y) * 0.5;
    const FLinearColor Filters[] = {FLinearColor::White, FLinearColor(1.0f, 0.83f, 0.64f),
        FLinearColor(0.66f, 0.85f, 1.0f), FLinearColor(0.64f, 0.65f, 0.66f)};
    DrawTexture(Texture, Center.X - W * 0.5, Center.Y - H * 0.5, W, H,
        Region.X, Region.Y, Region.Z - Region.X, Region.W - Region.Y, Filters[Edit.Filter],
        BLEND_Opaque, 1, false, Edit.Turn * 90, FVector2D(0.5, 0.5));
    return FBox2D(Center - Visible * 0.5, Center + Visible * 0.5);
}

int32 AReporterHUD::DrawStoryParagraphs(FString Value, FVector2D Position, double Width,
    float Scale, int32 MaxLines, int32 Page, int32* VisibleLines)
{
    Value.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
    Value.ReplaceInline(TEXT("\r"), TEXT("\n"));
    TArray<FString> Paragraphs;
    Value.ParseIntoArray(Paragraphs, TEXT("\n"), false);
    int32 Lines = 0;
    UFont* Font = GEngine->GetMediumFont();
    const auto Paint = [&](const FString& Text)
    {
        const int32 Row = Lines++ - Page * MaxLines;
        if (Row >= 0 && Row < MaxLines && !Text.IsEmpty())
        {
            DrawUiText(Text, Ink, Position.X, Position.Y + Row * 21 * Scale, Font, Scale);
        }
    };
    const auto Fits = [&](const FString& Text)
    {
        float W, H;
        MeasureUiText(Text, W, H, Font, Scale);
        return W <= Width;
    };
    for (const FString& Paragraph : Paragraphs)
    {
        TArray<FString> Words;
        Paragraph.ParseIntoArrayWS(Words);
        FString Row;
        for (const FString& Word : Words)
        {
            const FString Candidate = Row.IsEmpty() ? Word : Row + TEXT(" ") + Word;
            if (Fits(Candidate))
            {
                Row = Candidate;
                continue;
            }
            if (!Row.IsEmpty()) { Paint(Row); }
            Row = Word;
            int32 Split = 1;
            while (Split < Row.Len() && !Fits(Row))
            {
                while (Split < Row.Len() && Fits(Row.Left(Split + 1))) { ++Split; }
                Paint(Row.Left(Split));
                Row.RightChopInline(Split);
                Split = 1;
            }
        }
        Paint(Row);
    }
    if (VisibleLines) { *VisibleLines = FMath::Clamp(Lines - Page * MaxLines, 0, MaxLines); }
    return FMath::Max(1, FMath::DivideAndRoundUp(Lines, MaxLines));
}

int32 AReporterHUD::DrawNewspaper(const FNewspaper& Page, FVector2D Origin, FVector2D Size,
    double S, bool bPrint, int32 PageIndex, int32 PublicationDay)
{
    const bool bArchive = PublicationDay > 0;
    const auto Text = [&](const FString& Value, double X, double Y, FLinearColor Color, float Scale)
    {
        DrawUiText(Value, Color, Origin.X + X * S, Origin.Y + Y * S, GEngine->GetMediumFont(), Scale * S);
    };
    DrawRect(bPrint && Page.Paper ? FLinearColor(0.98f, 0.89f, 0.71f) : FLinearColor::White,
        Origin.X, Origin.Y, Size.X, Size.Y);
    Text(TEXT("Rivermere News"), 12, 15, Teal, 1.25f);
    DrawRect(Coral, Origin.X + 12 * S, Origin.Y + 47 * S, Size.X - 24 * S, 2 * S);
    if (Page.Stamp && bPrint)
    {
        Text(Page.Stamp == 1 ? TEXT("LOCAL") : TEXT("CORRECTION"), Size.X / S - 111, 23, Coral, 0.75f);
    }
    if (bArchive)
    {
        Text(FString::Printf(TEXT("DAY %d"), PublicationDay), 12, 55, Teal, 0.72f);
    }
    const double TitleY = bArchive ? 77 : 58;
    const double ContentWidth = Size.X - 24 * S;
    const float TitleScale = (bPrint ? 0.88f + Page.TitleSize * 0.18f : 1.0f) * S;
    int32 TitleLines = 0;
    const FVector2D Title = Origin + FVector2D(12, TitleY) * S;
    DrawStoryParagraphs(Page.Headline, Title, ContentWidth, TitleScale, 2, 0, &TitleLines);
    if (bPrint && Page.bBold)
    {
        DrawStoryParagraphs(Page.Headline, Title + FVector2D(0.65 * S, 0), ContentWidth, TitleScale, 2);
    }
    double ContentY = Title.Y + TitleLines * 21 * TitleScale + 8 * S;
    const double Left = Origin.X + 12 * S;
    if (bPrint && Page.bUnderline && !Page.Headline.TrimStartAndEnd().IsEmpty())
    {
        DrawRect(Ink, Left, ContentY, ContentWidth, S);
        ContentY += 13 * S;
    }
    if (!Page.Photos.IsEmpty())
    {
        const double MainWidth = ContentWidth * (Page.Layout ? 0.48 : 1.0);
        ContentY = DrawStoryPhoto(Page.Photos[0], Page.Edits[0], FVector2D(Left, ContentY),
            FVector2D(MainWidth, (bArchive ? 171 : 115) * S), true).Max.Y;
        const double SupplementY = ContentY + 6 * S;
        for (int32 Index = 1; Index < Page.Photos.Num(); ++Index)
        {
            const double X = Left + (Index - 1) * ContentWidth * 0.5;
            const FBox2D Bounds = DrawStoryPhoto(Page.Photos[Index], Page.Edits[Index],
                FVector2D(X, SupplementY), FVector2D(ContentWidth * 0.5 - 2 * S, 61 * S), true);
            ContentY = FMath::Max(ContentY, Bounds.Max.Y);
        }
        ContentY += 9 * S;
    }
    const float BodyScale = (0.92f + Page.BodySize * 0.14f) * S;
    const int32 Lines = FMath::Max(1, FMath::FloorToInt((Origin.Y + Size.Y - 9 * S - ContentY) / (21 * BodyScale)));
    return DrawStoryParagraphs(Page.Body, FVector2D(Left, ContentY), ContentWidth, BodyScale, Lines, PageIndex);
}

int32 AReporterHUD::GetGameDay() const
{
    return FMath::FloorToInt(OutdoorGameMinutes / 1440.0) + 1;
}

void AReporterHUD::SeedReaderComments(FPublishedArticle& Article, int32 ArticleIndex)
{
    if (!Article.Comments.IsEmpty()) { return; }
    const auto Add = [&](const FReaderLine& Reply, int32 Day)
    {
        Article.Comments.Add({Reply.Reader, Reply.Body, Day});
    };
    const int32 Event = Article.StoryType - 1;
    if (Event >= 0 && Event < UE_ARRAY_COUNT(EventComments) && Article.Angle >= 0 && Article.Angle < 3)
    {
        Add(EventComments[Event][Article.Angle], Article.Day);
    }
    const int32 Common = ArticleIndex % UE_ARRAY_COUNT(CommonComments);
    if (Article.Comments.IsEmpty() || Article.Comments[0].Reader != CommonComments[Common].Reader)
    {
        Add(CommonComments[Common], Article.Day);
    }
    else
    {
        Add(CommonComments[(Common + 1) % UE_ARRAY_COUNT(CommonComments)], Article.Day);
    }
    if (Article.bExposed) { Add(ExposureComment, Article.Day + 1); }
}

void AReporterHUD::UpdateReaderComments(FPublishedArticle& Article, int32 Day)
{
    if (Article.bPrintedEventFollowup || Day <= Article.Day) { return; }
    const TCHAR* Followup = nullptr;
    if (Article.StoryType == 1 && bToyReturned)
    {
        Followup = TEXT("I hear the little dinosaur made it home. That's a good end to the story.");
    }
    else if (Article.StoryType == 2 && Article.Angle == 1 && FallCases.IsValidIndex(Article.EventIndex))
    {
        const FFallCase& Incident = FallCases[Article.EventIndex];
        if (Incident.Cause == EFallCause::LooseTile && Incident.Site >= 0 &&
            Incident.Site < UE_ARRAY_COUNT(FallRepairDays))
        {
            const int32 Repaired = FallRepairDays[Incident.Site];
            if (Repaired > Article.Day && Repaired <= Day)
            {
                Followup = TEXT("I walked past this morning. The loose paving stone has been fixed.");
            }
        }
    }
    else if (Article.StoryType == 3 && LostCases.IsValidIndex(Article.EventIndex) && LostCases[Article.EventIndex].bReturned)
    {
        Followup = TEXT("Word is the wallet made it back to its owner. Good to hear.");
    }
    else if (Article.StoryType == 4 && Article.Angle == 1 && BirdFixDay > 0 && Day >= BirdFixDay)
    {
        Followup = TEXT("They put little white dots on the mall windows. I saw them this morning.");
    }
    if (Followup)
    {
        Article.Comments.Add({TEXT("Ruth at No. 8"), Followup, Day});
        Article.bPrintedEventFollowup = true;
    }
}

void AReporterHUD::AdvanceGameMinutes(double Minutes)
{
    if (bEndingOpen) { return; }
    const int32 PreviousDay = GetGameDay();
    const double TargetMinutes = OutdoorGameMinutes + Minutes;
    const int32 Today = FMath::FloorToInt(TargetMinutes / 1440.0) + 1;
    for (int32 Day = PreviousDay + 1; Day <= Today; ++Day)
    {
        OutdoorGameMinutes = (Day - 1) * 1440.0;
        for (FPublishedArticle& Article : PublishedArticles)
        {
            UpdateReaderComments(Article, Day);
            const int32 Age = Day - Article.Day;
            if (Age <= 0 || Age >= RevenueDays || (Article.bExposed && Age > 1)) { continue; }
            const int32 NewViews = Article.InitialViews >> Age;
            const int32 Earnings = NewViews / ViewsPerDollar;
            Article.Views += NewViews;
            Article.Earnings += Earnings;
            DailyArticleRevenue.FindOrAdd(Day) += Earnings;
            ReporterCash += Earnings;
            if (Age == 1 && Article.Angle == 0)
            {
                NewsroomFollowers += 2;
            }
            else if (Age == 1 && Article.Angle == 2 && FMath::FRand() < ExposureChance)
            {
                Article.bExposed = true;
                NewsroomFollowers = FMath::Max(0, NewsroomFollowers - ExposureFollowerLoss);
                Article.Comments.Add({ExposureComment.Reader, ExposureComment.Body, Day});
            }
        }
        if (SettleEndingDay(Day - 1)) { return; }
    }
    OutdoorGameMinutes = TargetMinutes;
}

void AReporterHUD::PublishNewspaper()
{
    FPublishedArticle& Article = PublishedArticles.AddDefaulted_GetRef();
    Article.Page = GetDraftNewspaper();
    Article.Day = GetGameDay();
    Article.Angle = StoryKind == 0 ? -1 : StoryAngle;
    Article.StoryType = StoryKind;
    Article.EventIndex = StoryKind == 2 ? StoryFallCase : StoryKind == 3 ? StoryWalletCase : INDEX_NONE;
    SeedReaderComments(Article, PublishedArticles.Num() - 1);
    const int32 BaseViews = StoryKind >= 2 ? 240 : Article.Angle < 0 ? FreeStoryBaseViews : FountainBaseViews;
    const double AngleMultiplier = Article.Angle < 0 ? 1.0 : AngleMultipliers[Article.Angle];
    const int32 EditingLevel = 1 + (FMath::Clamp(ReporterSkills[2], 1, 100) - 1) / 20;
    const double EditingMultiplier = 1.0 + (EditingLevel - 1) * 0.1;
    Article.InitialViews = FMath::RoundToInt((BaseViews + ViewsPerFollower * NewsroomFollowers)
        * AngleMultiplier * EditingMultiplier);
    Article.Views = Article.InitialViews;
    Article.Earnings = Article.Views / ViewsPerDollar;
    NewsroomFollowers += Article.Angle == -1 ? 1 : 3 + Article.Angle;
    DailyArticleRevenue.FindOrAdd(Article.Day) += Article.Earnings;
    ReporterCash += Article.Earnings;
    ReporterSkills[2] = FMath::Min(100, ReporterSkills[2] + 3);
    SelectedPublishedArticle = PublishedArticles.Num() - 1;
    NewsroomListPage = NewsroomBodyPage = 0;
    bNewsroomOverview = false;
    bNewsroomComments = false;
}

void AReporterHUD::DrawNewsroom(FVector2D Origin, double S, FVector2D Close)
{
    NewsroomActions.Reset();
    const auto At = [&](double X, double Y) { return Origin + FVector2D(X, Y) * S; };
    const auto Rect = [&](double X, double Y, double W, double H, FLinearColor Color)
    {
        const FVector2D P = At(X, Y);
        DrawRect(Color, P.X, P.Y, W * S, H * S);
    };
    const auto Text = [&](const FString& Value, double X, double Y, float Size = 13, FLinearColor Color = Ink, bool bBold = false)
    {
        UFont* Font = bBold && TitleBoldFont ? TitleBoldFont.Get() : GEngine->GetMediumFont();
        const float Scale = Size * S / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size);
        const FVector2D P = At(X, Y);
        DrawUiText(Value, Color, P.X, P.Y, Font, Scale);
    };
    const auto Centered = [&](const FString& Value, double X, double Y, double W, double H,
        float Size, FLinearColor Color)
    {
        UFont* Font = TitleBoldFont ? TitleBoldFont.Get() : GEngine->GetMediumFont();
        float Scale = Size * S / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size);
        float Width, Height;
        MeasureUiText(Value, Width, Height, Font, Scale);
        Scale *= FMath::Min(1.0, (W - 12) * S / FMath::Max(1.0f, Width));
        MeasureUiText(Value, Width, Height, Font, Scale);
        const FVector2D Center = At(X + W * 0.5, Y + H * 0.5);
        DrawUiText(Value, Color, Center.X - Width * 0.5, Center.Y - Height * 0.5, Font, Scale);
    };
    const auto Button = [&](int32 Action, const FString& Label, double X, double Y, double W, bool bActive = false)
    {
        Rect(X, Y, W, 25, bActive ? Coral : Teal);
        Centered(Label, X, Y, W, 25, 12, Pale);
        NewsroomActions.Emplace(FBox2D(At(X, Y), At(X + W, Y + 25)), Action);
    };
    const auto Panel = [&](double X, double Y, double W, double H)
    {
        Rect(X, Y, W, H, Line);
        Rect(X + 1, Y + 1, W - 2, H - 2, Pale);
    };

    Rect(0, 0, 800, 600, Cream);
    Rect(0, 0, 800, 46, Teal);
    Text(TEXT("Rivermere News - Newsroom"), 16, 9, 21, Pale, true);
    DrawRect(Coral, Close.X, Close.Y, 32 * S, 27 * S);
    Text(TEXT("X"), 765, 16, 13, Pale, true);
    Button(Overview, TEXT("Overview"), 12, 53, 90, bNewsroomOverview);
    Button(Articles, TEXT("Articles"), 106, 53, 86, !bNewsroomOverview);
    if (EndingIssue.Kind == EEndingKind::Special)
    {
        Button(SpecialEdition, TEXT("Day 30 Special"), 196, 53, 126);
    }
    if (!bNewsroomOverview && PublishedArticles.IsValidIndex(SelectedPublishedArticle))
    {
        const int32 Count = PublishedArticles[SelectedPublishedArticle].Comments.Num();
        Button(ShowStory, TEXT("Story"), 581, 74, !bNewsroomComments);
        Button(ShowComments, FString::Printf(TEXT("Comments (%d)"), Count), 661, 127, bNewsroomComments);
    }

    const int32 Today = GetGameDay();
    const int32 RentDays = (RentInterval - Today % RentInterval) % RentInterval;
    Panel(12, 86, 322, 56);
    Rect(102, 93, 1, 41, Line);
    Rect(241, 93, 1, 41, Line);
    const auto Stat = [&](const TCHAR* Label, const FString& Value, double X, double W, FLinearColor Color)
    {
        Centered(Label, X, 90, W, 21, 10, Ink);
        Centered(Value, X, 111, W, 26, 17, Color);
    };
    Stat(TEXT("Followers"), FString::FromInt(NewsroomFollowers), 13, 89, Teal);
    Stat(TEXT("Today's Revenue"), FString::Printf(TEXT("$%d"), DailyArticleRevenue.FindRef(Today)), 103, 138, Teal);
    Stat(TEXT("Rent Due"), RentDays ? FString::Printf(TEXT("In %dd"), RentDays) : FString(TEXT("Midnight")),
        242, 91, Coral);

    Panel(12, 150, 322, 163);
    Text(TEXT("Daily Revenue"), 21, 158, 16, Ink, true);
    Rect(206, 166, 9, 9, Teal);
    Text(TEXT("Article Revenue"), 220, 161, 10);
    const int32 FirstDay = FMath::Max(1, Today - 6);
    int32 Peak = 4;
    for (int32 Day = FirstDay; Day <= Today; ++Day)
    {
        Peak = FMath::Max(Peak, DailyArticleRevenue.FindRef(Day));
    }
    for (int32 Grid = 0; Grid < 3; ++Grid)
    {
        const double Y = 190 + Grid * 46;
        Rect(54, Y, 263, 1, Line);
        Text(FString::Printf(TEXT("$%d"), Peak * (2 - Grid) / 2), 20, Y - 7, 10);
    }
    for (int32 Day = FirstDay; Day <= Today; ++Day)
    {
        const int32 Earnings = DailyArticleRevenue.FindRef(Day);
        const double X = 57 + (Day - FirstDay) * 37;
        const double Height = 92.0 * Earnings / Peak;
        if (Earnings) { Rect(X + 4, 282 - Height, 25, Height, Teal); }
        Text(FString::Printf(TEXT("D%d"), Day), X + 4, 288, 10);
    }

    Panel(12, 321, 322, 267);
    Text(TEXT("Published Articles"), 21, 328, 16, Ink, true);
    if (PublishedArticles.IsEmpty())
    {
        Text(TEXT("Your published stories will appear here."), 21, 368, 12);
    }
    for (int32 Row = 0; Row < EntriesPerPage; ++Row)
    {
        const int32 Index = PublishedArticles.Num() - 1 - (NewsroomListPage * EntriesPerPage + Row);
        if (!PublishedArticles.IsValidIndex(Index)) { break; }
        const FPublishedArticle& Article = PublishedArticles[Index];
        const double Y = 356 + Row * 61;
        const bool bSelected = !bNewsroomOverview && SelectedPublishedArticle == Index;
        Rect(20, Y, 306, 56, bSelected ? Coral : Line);
        Rect(21, Y + 1, 304, 54, bSelected ? FLinearColor(0.98f, 0.82f, 0.68f) : Cream);
        DrawStoryPhoto(Article.Page.Photos[0], Article.Page.Edits[0], At(25, Y + 5), FVector2D(59, 46) * S, false);
        DrawStoryParagraphs(Article.Page.Headline, At(90, Y + 4), 226 * S, 0.76f * S, 2);
        Text(FString::Printf(TEXT("Day %d  |  %d views  |  $%d earned"), Article.Day, Article.Views, Article.Earnings),
            90, Y + 37, 10);
        NewsroomActions.Emplace(FBox2D(At(20, Y), At(326, Y + 56)), Index);
    }
    if (NewsroomListPage > 0) { Button(Newer, TEXT("<"), 21, 540, 30); }
    if ((NewsroomListPage + 1) * EntriesPerPage < PublishedArticles.Num())
    {
        Button(Older, TEXT(">"), 291, 540, 30);
    }
    Text(TEXT("Ad Revenue  $0"), 103, 551, 12, Ink, true);

    Panel(342, 86, 446, 502);
    if (bNewsroomOverview)
    {
        Text(TEXT("Revenue Overview"), 359, 103, 22, Teal, true);
        Text(TEXT("Day"), 360, 151, 13, Ink, true);
        Text(TEXT("Article Revenue"), 485, 151, 13, Ink, true);
        Text(TEXT("Ad Revenue"), 671, 151, 13, Ink, true);
        for (int32 Day = FirstDay; Day <= Today; ++Day)
        {
            const double Y = 183 + 30 * (Day - FirstDay);
            Rect(359, Y - 4, 411, 1, Line);
            Text(FString::Printf(TEXT("Day %d"), Day), 360, Y);
            Text(FString::Printf(TEXT("$%d"), DailyArticleRevenue.FindRef(Day)), 485, Y);
            Text(TEXT("$0"), 671, Y);
        }
        Text(TEXT("Articles earn less each day after publication."), 359, 425, 13, Teal);
        return;
    }
    if (!PublishedArticles.IsValidIndex(SelectedPublishedArticle))
    {
        Text(TEXT("No published articles yet."), 360, 111, 18, Ink, true);
        return;
    }
    const FPublishedArticle& Article = PublishedArticles[SelectedPublishedArticle];
    if (bNewsroomComments)
    {
        Text(TEXT("Reader Comments"), 356, 100, 21, Teal, true);
        DrawStoryParagraphs(Article.Page.Headline, At(356, 131), 416 * S, 0.72f * S, 2);
        for (int32 Index = 0; Index < Article.Comments.Num(); ++Index)
        {
            const FPublishedArticle::FComment& Comment = Article.Comments[Index];
            const double Y = 179 + Index * 87;
            Panel(354, Y, 422, 79);
            Text(Comment.Reader, 366, Y + 5, 13, Teal, true);
            Text(FString::Printf(TEXT("Day %d"), Comment.Day), 713, Y + 6, 11);
            DrawStoryParagraphs(Comment.Body, At(366, Y + 27), 398 * S, 0.73f * S, 3);
        }
        if (Article.bExposed)
        {
            Text(TEXT("Misleading story exposed. Lost followers and future traffic."), 355, 550, 11, Coral, true);
        }
        return;
    }
    const int32 Pages = DrawNewspaper(Article.Page, At(344, 88), FVector2D(442, 434) * S, S, true, NewsroomBodyPage, Article.Day);
    if (bShowFirstStoryResults)
    {
        Text(TEXT("First story published!"), 354, 526, 11, Teal, true);
    }
    Text(FString::Printf(TEXT("%d views  |  $%d earned"), Article.Views, Article.Earnings), 354, 546, 12, Ink, true);
    if (Article.bExposed)
    {
        Text(TEXT("Misleading story exposed. Lost followers and future traffic."), 354, 570, 10, Coral);
    }
    if (Pages > 1)
    {
        if (NewsroomBodyPage > 0) { Button(PreviousPage, TEXT("<"), 660, 550, 30); }
        if (NewsroomBodyPage + 1 < Pages) { Button(NextPage, TEXT(">"), 748, 550, 30); }
        Text(FString::Printf(TEXT("%d / %d"), NewsroomBodyPage + 1, Pages), 700, 553, 11);
    }
}

bool AReporterHUD::ClickNewsroom(FVector2D Mouse)
{
    for (const auto& Entry : NewsroomActions)
    {
        if (!Entry.Key.IsInsideOrOn(Mouse)) { continue; }
        switch (Entry.Value)
        {
        case ShowStory: bNewsroomComments = false; break;
        case ShowComments: bNewsroomComments = true; break;
        case SpecialEdition: OpenEnding(true); break;
        case Overview: bNewsroomOverview = true; break;
        case Articles:
            bNewsroomOverview = false;
            bNewsroomComments = false;
            if (!PublishedArticles.IsValidIndex(SelectedPublishedArticle) && !PublishedArticles.IsEmpty())
            {
                SelectedPublishedArticle = PublishedArticles.Num() - 1;
                NewsroomListPage = NewsroomBodyPage = 0;
            }
            break;
        case Older: ++NewsroomListPage; break;
        case Newer: --NewsroomListPage; break;
        case PreviousPage: --NewsroomBodyPage; break;
        case NextPage: ++NewsroomBodyPage; break;
        default:
            SelectedPublishedArticle = Entry.Value;
            NewsroomBodyPage = 0;
            bNewsroomOverview = false;
            bNewsroomComments = false;
            break;
        }
        NewsroomActions.Reset();
        return true;
    }
    return false;
}
