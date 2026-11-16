#include "ReporterGameMode.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "PaperSprite.h"

namespace
{
const FName FountainSprite(TEXT("SP_Fountain"));
const FLinearColor Ink(0.11f, 0.07f, 0.05f);
const FLinearColor Cream(0.98f, 0.91f, 0.75f);
const FLinearColor Teal(0.10f, 0.34f, 0.31f);
const TCHAR* Names[] = {
    TEXT("Eli"), TEXT("Nora"), TEXT("Mrs. Brooks"), TEXT("Mr. Bennett"),
    TEXT("Maya"), TEXT("Jamie"), TEXT("Lucy")
};
const TCHAR* Locations[] = {TEXT("the park walkway"), TEXT("the park entrance")};
const TCHAR* Quotes[] = {
    TEXT("I lost my footing. Give me a second."),
    TEXT("I'm all right. That startled me!"),
    TEXT("Thank you, dear. I'll sit a moment."),
    TEXT("I'm all right. Let me catch my breath."),
    TEXT("I'm okay. I just need a minute."),
    TEXT("I'm okay. I wish my dad was here."),
    TEXT("I'm okay. That was scary.")
};
constexpr double SiteOffsets[] = {-375.0, 425.0};
}

bool AReporterHUD::IsRainyDay() const
{
    return GetGameDay() % 4 == 3;
}

void AReporterHUD::UpdateFallEvent()
{
    const int32 Day = GetGameDay();
    if (ActiveFallCase != INDEX_NONE || Day < NextFallDay || LastFallRollDay == Day ||
        FallSeatedPoses.Num() != UE_ARRAY_COUNT(Names))
    {
        return;
    }
    LastFallRollDay = Day;
    const float Chance = FallCases.IsEmpty() ? 0.8f : 0.25f;
    if (FMath::FRand() >= Chance)
    {
        return;
    }
    for (const AParkPropActor* Prop : ParkProps)
    {
        if (!IsValid(Prop) || !Prop->Artwork || Prop->Artwork->GetFName() != FountainSprite)
        {
            continue;
        }
        const int32 Site = FMath::RandRange(0, UE_ARRAY_COUNT(SiteOffsets) - 1);
        TArray<EFallCause, TInlineAllocator<3>> Causes = {EFallCause::Misstep};
        if (FallRepairDays[Site] == 0 || Day < FallRepairDays[Site])
        {
            Causes.Add(EFallCause::LooseTile);
        }
        if (IsRainyDay())
        {
            Causes.Add(EFallCause::WetPaving);
        }
        ActiveFallCase = FallCases.AddDefaulted();
        FFallCase& Incident = FallCases[ActiveFallCase];
        Incident.Day = Day;
        Incident.Site = Site;
        Incident.Resident = FMath::RandRange(0, UE_ARRAY_COUNT(Names) - 1);
        Incident.Ground = Prop->GetBase() + FVector2D(SiteOffsets[Site], 59);
        Incident.Ground.Y = FMath::Clamp(Incident.Ground.Y, 155.0, 239.0);
        Incident.Cause = Causes[FMath::RandRange(0, Causes.Num() - 1)];
        Incident.Crowd = static_cast<EFallCrowd>(FMath::RandRange(0, 2));
        if (Incident.Crowd == EFallCrowd::Helping)
        {
            Incident.Witness = Incident.Resident == 0 ? 1 : 0;
        }
        else if (Incident.Crowd == EFallCrowd::Witness)
        {
            const int32 Choice = FMath::RandRange(0, UE_ARRAY_COUNT(Names) - 2);
            Incident.Witness = Choice + (Choice >= Incident.Resident ? 1 : 0);
        }
        break;
    }
}

void AReporterHUD::LeaveFallSceneForBuilding()
{
    if (!FallCases.IsValidIndex(ActiveFallCase) || !FallPhotoFiles.FindKey(ActiveFallCase))
    {
        return;
    }
    NextFallDay = GetGameDay() + 4;
    ActiveFallCase = INDEX_NONE;
    SetFindNotice(TEXT("Scene closed. Photos and notes are ready in the Editor."));
}

bool AReporterHUD::IsFallParticipant(int32 Character) const
{
    if (!FallCases.IsValidIndex(ActiveFallCase))
    {
        return false;
    }
    const FFallCase& Incident = FallCases[ActiveFallCase];
    return Character == Incident.Resident || Character == Incident.Witness;
}

void AReporterHUD::DrawFallScene(FVector2D Center, double CameraX, double Scale, const FLinearColor& Tint)
{
    FallPhotoSubject = FBox2D(ForceInit);
    const auto Project = [&](FVector2D Point) { return Center + (Point - FVector2D(CameraX, 0)) * Scale; };
    const auto DrawTile = [&](FVector2D P, bool bFixed)
    {
        DrawRect(bFixed ? FLinearColor(0.83f, 0.65f, 0.49f) : FLinearColor(0.34f, 0.28f, 0.24f),
            P.X - 25 * Scale, P.Y - 7 * Scale, 51 * Scale, 13 * Scale);
        DrawRect(bFixed ? FLinearColor(0.91f, 0.74f, 0.59f) : FLinearColor(0.72f, 0.53f, 0.38f),
            P.X - 23 * Scale, P.Y - 7 * Scale, 47 * Scale, (bFixed ? 11 : 8) * Scale);
        if (!bFixed)
        {
            DrawLine(P.X - 16 * Scale, P.Y - 6 * Scale, P.X + 10 * Scale, P.Y + 3 * Scale,
                Ink, FMath::Max(1.0, Scale));
        }
    };
    for (int32 Site = 0; Site < UE_ARRAY_COUNT(FallRepairDays); ++Site)
    {
        if (FallRepairDays[Site] > 0 && GetGameDay() >= FallRepairDays[Site])
        {
            for (const FFallCase& Past : FallCases)
            {
                if (Past.Site == Site && Past.Cause == EFallCause::LooseTile)
                {
                    DrawTile(Project(Past.Ground + FVector2D(-74, -8)), true);
                    break;
                }
            }
        }
    }
    if (!FallCases.IsValidIndex(ActiveFallCase))
    {
        return;
    }

    const FFallCase& Incident = FallCases[ActiveFallCase];
    const FVector2D Foot = Project(Incident.Ground);
    const FVector2D Evidence = Project(Incident.Ground + FVector2D(-74, -8));
    if (Incident.Cause == EFallCause::LooseTile)
    {
        DrawTile(Evidence, false);
    }
    else if (Incident.Cause == EFallCause::WetPaving)
    {
        const FLinearColor Water(0.38f, 0.56f, 0.60f);
        DrawRect(Water, Evidence.X - 30 * Scale, Evidence.Y - 3 * Scale, 59 * Scale, 8 * Scale);
        DrawRect(Water, Evidence.X - 19 * Scale, Evidence.Y - 6 * Scale, 31 * Scale, 13 * Scale);
        DrawLine(Evidence.X - 14 * Scale, Evidence.Y, Evidence.X + 16 * Scale, Evidence.Y,
            FLinearColor(0.64f, 0.76f, 0.77f), Scale);
    }

    const double SeatedHeight = (Incident.Resident >= 5 ? 102.0 : 131.0) * Scale;
    if (UTexture2D* Sprite = FallSeatedPoses[Incident.Resident])
    {
        const double Width = SeatedHeight * Sprite->GetSizeX() / Sprite->GetSizeY();
        const FVector2D Min = Foot - FVector2D(Width * 0.5, SeatedHeight);
        FallPhotoSubject = FBox2D(Min, Foot + FVector2D(Width * 0.5, 0));
        DrawTexture(Sprite, Min.X, Min.Y, Width, SeatedHeight, 0, 0, 1, 1, Tint, BLEND_Translucent);
    }
    FallScreenAnchor = Foot - FVector2D(0, SeatedHeight + 22 * Scale);
    if (PedestrianAtlases.IsValidIndex(Incident.Witness) && PedestrianAtlases[Incident.Witness])
    {
        UTexture2D* Atlas = PedestrianAtlases[Incident.Witness];
        const int32 Pose = Incident.Crowd == EFallCrowd::Helping && Incident.Witness == 0 ? 3 : 0;
        const double Height = (Incident.Witness >= 5 ? 131.0 : 181.0) * Scale;
        const double Width = Height * 288.0 / Atlas->GetSizeY();
        const double FrameU = 288.0 / Atlas->GetSizeX();
        const FVector2D End = Foot + FVector2D((Pose ? 97 : 127) * Scale, 0);
        DrawTexture(Atlas, End.X - Width / 2, End.Y - Height, Width, Height,
            Pose * FrameU, 0, FrameU, 1, Tint, BLEND_Translucent);
    }
}

void AReporterHUD::DrawFallPrompts(FVector2D Ground)
{
    if (!FallCases.IsValidIndex(ActiveFallCase) || bReporterPanelOpen || bBackpackOpen)
    {
        return;
    }
    const FFallCase& Incident = FallCases[ActiveFallCase];
    const FVector2D Distance = Incident.Ground - Ground;
    if (FMath::Abs(Distance.X) > 205 || FMath::Abs(Distance.Y) > 140 ||
        FallScreenAnchor.X < 0 || FallScreenAnchor.X > Canvas->ClipX)
    {
        return;
    }
    const double S = GetHudScale();
    UFont* Font = GEngine->GetMediumFont();
    const auto Button = [&](EFallAction Action, const TCHAR* Label, FVector2D Pos, double Width)
    {
        const FVector2D Size(Width * S, 29 * S);
        DrawRect(Teal, Pos.X, Pos.Y, Size.X, Size.Y);
        DrawUiText(Label, Cream, Pos.X + 8 * S, Pos.Y + 5 * S, Font, 0.86f * S);
        FallActions.Emplace(FBox2D(Pos, Pos + Size), Action);
    };
    if (!Incident.bPanelOpen)
    {
        Button(EFallAction::Inspect, TEXT("?"), FallScreenAnchor - FVector2D(12 * S, 0), 28);
        return;
    }

    const double Width = 430 * S;
    const FVector2D Pos((Canvas->ClipX - Width) * 0.5, 111 * S);
    DrawRect(Ink, Pos.X, Pos.Y, Width, 143 * S);
    DrawRect(Cream, Pos.X + 2 * S, Pos.Y + 2 * S, Width - 4 * S, 139 * S);
    DrawUiText(FString::Printf(TEXT("Fall at %s"), Locations[Incident.Site]), Teal,
        Pos.X + 12 * S, Pos.Y + 8 * S, Font, 1.1f * S);

    FString Note = TEXT("The resident is still sitting on the pavement.");
    if (Incident.bCheckedGround)
    {
        Note = Incident.Cause == EFallCause::LooseTile ? TEXT("A loose tile sticks up beside the resident.")
            : Incident.Cause == EFallCause::WetPaving ? TEXT("There is a slick patch of wet paving here.")
            : TEXT("The paving looks even and clear.");
    }
    if (Incident.bSpoke)
    {
        if (HasVerifiedFallCause(Incident))
        {
            Note = Incident.Cause == EFallCause::LooseTile ? TEXT("Confirmed: the resident caught a foot on the loose tile.")
                : Incident.Cause == EFallCause::WetPaving ? TEXT("Confirmed: the resident slipped on wet paving.")
                : TEXT("Confirmed: the resident lost their footing.");
        }
        else
        {
            Note = Quotes[Incident.Resident];
        }
    }
    DrawUiText(Note, Ink, Pos.X + 12 * S, Pos.Y + 36 * S, Font, 0.86f * S);
    Button(EFallAction::Camera, TEXT("Take a photo"), Pos + FVector2D(12, 69) * S, 190);
    Button(EFallAction::CheckResident, TEXT("Check on resident"), Pos + FVector2D(218, 69) * S, 200);
    Button(EFallAction::CheckGround, TEXT("Inspect the ground"), Pos + FVector2D(12, 105) * S, 190);
    Button(EFallAction::ClosePanel, TEXT("Close"), Pos + FVector2D(218, 105) * S, 200);
}

bool AReporterHUD::ClickFallPrompts(FVector2D Pointer)
{
    if (!FallCases.IsValidIndex(ActiveFallCase))
    {
        return false;
    }
    FFallCase& Incident = FallCases[ActiveFallCase];
    for (const auto& Entry : FallActions)
    {
        if (!Entry.Key.IsInsideOrOn(Pointer)) { continue; }
        const EFallAction Action = Entry.Value;
        FallActions.Reset();
        switch (Action)
        {
        case EFallAction::Inspect:
            Incident.bDiscovered = true;
            Incident.bPanelOpen = true;
            break;
        case EFallAction::Camera: ToggleCamera(); break;
        case EFallAction::CheckResident:
            if (!Incident.bSpoke)
            {
                ++ReporterMorality;
                ReporterSkills[0] = FMath::Min(100, ReporterSkills[0] + 4);
            }
            Incident.bSpoke = true;
            if (Incident.Cause == EFallCause::LooseTile) { SetFindNotice(TEXT("Resident: My shoe caught on something.")); }
            else if (Incident.Cause == EFallCause::WetPaving) { SetFindNotice(TEXT("Resident: My foot slipped on the wet stones.")); }
            else { SetFindNotice(Quotes[Incident.Resident]); }
            break;
        case EFallAction::CheckGround:
            if (!Incident.bCheckedGround) { ReporterSkills[1] = FMath::Min(100, ReporterSkills[1] + 4); }
            Incident.bCheckedGround = true;
            break;
        case EFallAction::ClosePanel: Incident.bPanelOpen = false; break;
        }
        return true;
    }
    return false;
}

bool AReporterHUD::HasVerifiedFallCause(const FFallCase& Incident) const
{
    return Incident.bSpoke && (Incident.Cause == EFallCause::Misstep || Incident.bCheckedGround);
}

FString AReporterHUD::FallStoryLabel(int32 Index) const
{
    const FFallCase& Incident = FallCases[Index];
    return FString::Printf(TEXT("Fall: %s (Day %d)"), Names[Incident.Resident], Incident.Day);
}

void AReporterHUD::ApplyFallTemplate()
{
    const FFallCase& Incident = FallCases[StoryFallCase];
    const TCHAR* Name = Names[Incident.Resident];
    const TCHAR* Place = Locations[Incident.Site];
    const TCHAR* Assistance = Incident.Crowd == EFallCrowd::Helping ? TEXT("A passerby stopped to help.")
        : Incident.Crowd == EFallCrowd::Witness ? TEXT("Another resident witnessed the fall.")
        : TEXT("No other pedestrians were nearby.");
    if (StoryAngle == 0)
    {
        DraftHeadline = TEXT("Resident Falls in Rivermere Park");
        DraftBody = FString::Printf(TEXT("%s fell near %s. %s"), Name, Place, Assistance);
        if (!HasVerifiedFallCause(Incident))
        {
            DraftBody += TEXT(" The cause is unconfirmed.");
        }
    }
    else if (StoryAngle == 1)
    {
        const TCHAR* Headlines[] = {
            TEXT("Resident Explains Park Fall"), TEXT("Loose Tile Trips Park Visitor"), TEXT("Wet Paving Causes Park Fall")
        };
        const TCHAR* Facts[] = {
            TEXT("The resident said they lost their footing."),
            TEXT("The resident caught a foot on a raised tile. We confirmed the defect on site."),
            TEXT("The resident slipped on wet paving. We observed a slick patch at the scene.")
        };
        const int32 Cause = static_cast<int32>(Incident.Cause);
        DraftHeadline = Headlines[Cause];
        DraftBody = FString::Printf(TEXT("%s fell near %s. %s %s"), Name, Place, Facts[Cause], Assistance);
    }
    else if (Incident.Crowd == EFallCrowd::Helping)
    {
        DraftHeadline = TEXT("Park Argument Ends in a Fall!");
        DraftBody = FString::Printf(TEXT("A tense confrontation broke out near %s. A photograph shows a passerby reaching for %s just after the fall."), Place, Name);
    }
    else if (Incident.Crowd == EFallCrowd::Witness)
    {
        DraftHeadline = TEXT("Onlookers Stand By as Resident Falls!");
        DraftBody = FString::Printf(TEXT("%s fell near %s while a bystander looked on. Has Rivermere stopped caring?"), Name, Place);
    }
    else
    {
        DraftHeadline = TEXT("Resident Left Alone After Park Fall!");
        DraftBody = FString::Printf(TEXT("%s fell near %s. No one came to help for what seemed like ages."), Name, Place);
    }
    bCustomStory = false;
    BodyPage = 0;
    HideStoryInputs();
}

bool AReporterHUD::CanPublishFallStory() const
{
    return StoryKind != 2 || (FallCases.IsValidIndex(StoryFallCase) && !FallCases[StoryFallCase].bPublished &&
        ArticlePhotos.ContainsByPredicate([&](int32 Photo)
        {
            const int32* Index = FallPhotoCases.Find(Photo);
            return Index && *Index == StoryFallCase;
        }));
}

void AReporterHUD::PublishFallStory()
{
    FFallCase& Incident = FallCases[StoryFallCase];
    Incident.bPublished = true;
    if (StoryAngle == 1 && Incident.Cause == EFallCause::LooseTile && FallRepairDays[Incident.Site] == 0)
    {
        FallRepairDays[Incident.Site] = GetGameDay() + 2;
    }
}
