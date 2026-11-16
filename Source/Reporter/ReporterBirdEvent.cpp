#include "ReporterGameMode.h"

#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "PaperSprite.h"

namespace
{
const AParkPropActor* FindMall(const TArray<TObjectPtr<AParkPropActor>>& Props)
{
    const FName MallSprite(TEXT("SP_RivermereMall"));
    for (const AParkPropActor* Prop : Props)
    {
        if (IsValid(Prop) && Prop->Artwork && Prop->Artwork->GetFName() == MallSprite) { return Prop; }
    }
    return nullptr;
}

FVector2D BirdGround(const AParkPropActor& Mall)
{
    return Mall.GetBase() + FVector2D(Mall.GetDrawSize().X * 0.27, 45);
}
}

bool AReporterHUD::IsBirdSceneActive() const
{
    return BirdFixDay == 0 && BirdReports < 2 && GetGameDay() >= BirdNextDay;
}

void AReporterHUD::UpdateBirdScene(FVector2D Ground)
{
    if (!IsBirdSceneActive() || !InjuredBirdArt) { return; }
    const AParkPropActor* Mall = FindMall(ParkProps);
    if (!Mall || FVector2D::DistSquared(Ground, BirdGround(*Mall)) > FMath::Square(175.0)) { return; }
    const bool bRecognizedGlass = BirdReports == 1 || ReporterSkills[1] >= 41;
    if (BirdSeenRound == BirdReports + 1 && (!bRecognizedGlass || bBirdGlassKnown)) { return; }
    BirdSeenRound = BirdReports + 1;
    bBirdGlassKnown = bRecognizedGlass;
    SetFindNotice(bBirdGlassKnown ? TEXT("The bird struck the glass. Photograph it for the story. [C]")
        : TEXT("An injured bird. What happened? Take a photo. [C]"));
}

void AReporterHUD::DrawBirdScene(FVector2D Center, double CameraX, double Scale, FLinearColor Tint)
{
    BirdPhotoBounds = FBox2D(ForceInit);
    if (!IsBirdSceneActive() || !InjuredBirdArt) { return; }
    const AParkPropActor* Mall = FindMall(ParkProps);
    if (!Mall) { return; }
    const FVector2D Foot = Center + (BirdGround(*Mall) - FVector2D(CameraX, 0)) * Scale;
    const double W = 58 * Scale;
    const double H = W * InjuredBirdArt->GetSizeY() / InjuredBirdArt->GetSizeX();
    const FVector2D Min = Foot - FVector2D(W * 0.5, H);
    BirdPhotoBounds = FBox2D(Min, Min + FVector2D(W, H));
    DrawTexture(InjuredBirdArt, Min.X, Min.Y, W, H, 0, 0, 1, 1, Tint, BLEND_Translucent);
    const FString Suffix = FString::Printf(TEXT("_Bird%d.png"), BirdReports + 1);
    if (!bCameraMode && !bReporterPanelOpen && BirdSeenRound == BirdReports + 1 &&
        !CapturedPhotoFiles.ContainsByPredicate([&](const FString& File) { return File.EndsWith(Suffix); }))
    {
        DrawUiText(TEXT("?"), FLinearColor::White, Foot.X - 5 * Scale, Min.Y - 31 * Scale,
            GEngine->GetMediumFont(), 1.25 * Scale);
    }
}

int32 AReporterHUD::GetBirdPhotoIndex() const
{
    if (!IsBirdSceneActive()) { return INDEX_NONE; }
    const FString Suffix = FString::Printf(TEXT("_Bird%d.png"), BirdReports + 1);
    for (int32 Index = EditorPhotoFiles.Num() - 1; Index >= 0; --Index)
    {
        if (EditorPhotoFiles[Index].EndsWith(Suffix)) { return Index; }
    }
    return INDEX_NONE;
}

bool AReporterHUD::CanPublishBirdStory() const
{
    if (StoryKind != 4) { return true; }
    if (!IsBirdSceneActive()) { return false; }
    const FString Suffix = FString::Printf(TEXT("_Bird%d.png"), BirdReports + 1);
    return ArticlePhotos.ContainsByPredicate([&](int32 Index)
    {
        return EditorPhotoFiles.IsValidIndex(Index) && EditorPhotoFiles[Index].EndsWith(Suffix);
    });
}

void AReporterHUD::ApplyBirdTemplate()
{
    StoryAngle = (bBirdGlassKnown || BirdReports == 1) ? 1 : 0;
    if (StoryAngle == 1)
    {
        DraftHeadline = TEXT("Mall Windows Linked to Injured Birds");
        DraftBody = TEXT("A bird was found injured beneath the glass at Rivermere Mall. From the pavement, "
            "the windows reflect the sky and trees beside the entrance. The newsroom traced the strike "
            "to the glass and asked mall management to make the panes more visible to birds.");
    }
    else
    {
        DraftHeadline = TEXT("Injured Bird Found Outside Rivermere Mall");
        DraftBody = TEXT("A small bird was found injured on the pavement outside Rivermere Mall. "
            "The cause of its injuries is still unknown. The newsroom photographed the scene "
            "and will keep looking for clues.");
    }
}

void AReporterHUD::PublishBirdStory()
{
    ++BirdReports;
    if (StoryAngle == 1)
    {
        bBirdGlassKnown = true;
        BirdFixDay = GetGameDay() + 1;
    }
    else
    {
        BirdNextDay = GetGameDay() + 3;
    }
}
