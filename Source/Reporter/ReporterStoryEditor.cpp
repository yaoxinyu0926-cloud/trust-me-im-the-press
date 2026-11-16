#include "ReporterGameMode.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

namespace
{
const FLinearColor Ink(0.11f, 0.085f, 0.056f);
const FLinearColor Teal(0.08f, 0.24f, 0.23f);
const FLinearColor Cream(0.92f, 0.83f, 0.66f);
const FLinearColor Coral(0.62f, 0.20f, 0.12f);
const TCHAR* const Angles[] = {TEXT("Factual"), TEXT("Questioning"), TEXT("Sensational")};
const TCHAR* const Headlines[3][2] = {
    {TEXT("Toy Dinosaur Found in Park Fountain"), TEXT("An Afternoon at Rivermere Park")},
    {TEXT("What Is in the Fountain?"), TEXT("A Mystery at Rivermere Park")},
    {TEXT("River Monster Spotted!"), TEXT("A Creature Has Invaded Our Park!")}
};
const TCHAR* const Bodies[] = {
    TEXT("A small green dinosaur toy was spotted in the fountain at Rivermere Park."),
    TEXT("An unusual shape has caught visitors' attention. Our newsroom is looking for more information."),
    TEXT("A mysterious creature has appeared at the park fountain. Witnesses are urging visitors to keep their distance.")
};
enum EEditorAction
{
    Tab = 1, PreviousPhotos = 5, NextPhotos, AddPhoto, RemovePhoto, MakeLead, NextAngle,
    NextTemplate, NextLayout, CropPhoto, RotatePhoto, FilterPhoto, ResetPhoto,
    PublishArticle, NextSize, ToggleBold, ToggleUnderline, NextPaper, NextStamp,
    SaveFrontPage, FreeStory, EventStory, ApplyDraftPhoto, CopyPhoto, NextBodySize, PreviousBody, NextBody, ApplyTemplate, FallListPrevious, FallListNext, ChooseStory, BirdStory, SelectPhoto = 100, SelectArticlePhoto = 200, SelectFallStory = 300, SelectWalletStory = 10000
};
}

void AReporterHUD::RefreshEditorPhotos()
{
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Photos");
    for (const FString& File : CapturedPhotoFiles)
    {
        if (LoadedPhotoFiles.Contains(File)) { continue; }
        if (UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(Directory / File))
        {
            EditorPhotos.Add(Texture);
            EditorPhotoFiles.Add(File);
            LoadedPhotoFiles.Add(File);
            if (const int32* CaseIndex = FallPhotoFiles.Find(File); CaseIndex && FallCases.IsValidIndex(*CaseIndex))
            {
                const int32 Index = EditorPhotos.Num() - 1;
                FallPhotoCases.Add(Index, *CaseIndex);
                FallCases[*CaseIndex].LatestPhoto = Index;
            }
            if (const int32* CaseIndex = WalletPhotoFiles.Find(File); CaseIndex && LostCases.IsValidIndex(*CaseIndex))
            {
                const int32 Index = EditorPhotos.Num() - 1;
                WalletPhotoCases.Add(Index, *CaseIndex);
                LostCases[*CaseIndex].LatestPhoto = Index;
            }
            if (File.EndsWith(TEXT("_FountainToy.png")))
            {
                const int32 Index = EditorPhotos.Num() - 1;
                FountainPhotos.Add(Index);
                if (LatestImportedFountainPhoto.IsEmpty() || File > LatestImportedFountainPhoto)
                {
                    LatestImportedFountainPhoto = File;
                    LatestFountainPhotoIndex = Index;
                }
                EditorPhotoNames.Add(TEXT("Fountain Event"));
            }
            else
            {
                const int32* CaseIndex = FallPhotoFiles.Find(File);
                EditorPhotoNames.Add(CaseIndex ? FallStoryLabel(*CaseIndex)
                    : WalletPhotoFiles.Contains(File) ? FString(TEXT("Wallet scene"))
                    : (File.EndsWith(TEXT("_Bird1.png")) || File.EndsWith(TEXT("_Bird2.png"))) ? FString(TEXT("Bird at the mall"))
                    : FString::Printf(TEXT("Photo %d"), LoadedPhotoFiles.Num()));
            }
        }
    }
    EditorPhotoEdits.SetNum(EditorPhotos.Num());
    DraftPhotoEdits.SetNum(EditorPhotos.Num());
}

void AReporterHUD::DrawStoryEditor(FVector2D Origin, FVector2D Size, FVector2D Close, FVector2D CloseSize)
{
    EditorActions.Reset();
    CropCanvas = FBox2D(ForceInit);
    const double S = Size.X / 800.0;
    if (!bSettingsOpen && StoryKind != INDEX_NONE && StoryKind < 2 && EditorTab == 0) { ShowStoryInputs(Origin, S); }
    else { HideStoryInputs(); }
    const auto Rect = [&](double X, double Y, double W, double H, FLinearColor Color)
    {
        DrawRect(Color, Origin.X + X * S, Origin.Y + Y * S, W * S, H * S);
    };
    const auto Text = [&](const FString& Value, double X, double Y, FLinearColor Color = Ink, float Magnify = 1.0f)
    {
        DrawUiText(Value, Color, Origin.X + X * S, Origin.Y + Y * S, GEngine->GetMediumFont(), S * Magnify);
    };
    const auto Button = [&](int32 Action, const FString& Label, double X, double Y, double W, bool bActive = false)
    {
        Rect(X, Y, W, 27, bActive ? Coral : Teal);
        Text(Label, X + 7, Y + 5, FLinearColor::White, 0.85f);
        EditorActions.Emplace(FBox2D(Origin + FVector2D(X, Y) * S,
            Origin + FVector2D(X + W, Y + 27) * S), Action);
    };
    const auto Photo = [&](int32 Index, double X, double Y, double W, double H,
        bool bDraft = false, bool bCropSource = false) -> double
    {
        if (!EditorPhotos.IsValidIndex(Index)) { return Y; }
        const FEditorPhoto Edit = bCropSource ? FEditorPhoto{FVector4(0, 0, 1, 1), 0, WorkingPhoto.Filter}
            : (bDraft ? DraftPhotoEdits[Index] : EditorPhotoEdits[Index]);
        const FBox2D Bounds = DrawStoryPhoto(Index, Edit, Origin + FVector2D(X, Y) * S,
            FVector2D(W, H) * S, bDraft);
        if (bCropSource) { CropCanvas = Bounds; }
        return (Bounds.Max.Y - Origin.Y) / S;
    };
    DrawRect(Coral, Close.X, Close.Y, CloseSize.X, CloseSize.Y);
    DrawUiText(TEXT("X"), FLinearColor::White, Close.X + 10 * S, Close.Y + 3 * S, GEngine->GetSmallFont(), S);
    if (StoryKind == INDEX_NONE)
    {
        Text(TEXT("Choose a story"), 232, 141, Ink, 1.35f);
        Button(FreeStory, TEXT("Free Story"), 232, 205, 335);
        Text(TEXT("Write an article with your own photos."), 232, 240);
        Text(TEXT("Event Stories"), 232, 289, Teal);
        if (HasPendingFountainStory())
        {
            const bool bGuideEvent = bFirstStoryGuide && !LatestCapturedFountainPhoto.IsEmpty();
            Button(EventStory, TEXT("Toy in the Fountain"), 232, 325, 335, bGuideEvent);
            if (bGuideEvent)
            {
                Text(TEXT("Select your fountain story to start editing."), 232, 366);
            }
        }
        TArray<int32> PendingFalls;
        if (GetBirdPhotoIndex() != INDEX_NONE) { PendingFalls.Add(INDEX_NONE); }
        for (int32 Index = FallCases.Num() - 1; Index >= 0; --Index)
        {
            if (!FallCases[Index].bPublished && FallCases[Index].LatestPhoto != INDEX_NONE)
            {
                PendingFalls.Add(Index);
            }
        }
        for (int32 Index = LostCases.Num() - 1; Index >= 0; --Index)
        {
            if (LostCases[Index].LatestPhoto != INDEX_NONE && !LostCases[Index].bPublished)
            {
                PendingFalls.Add(SelectWalletStory + Index);
            }
        }
        constexpr int32 FallsPerPage = 3;
        FallStoryPage = FMath::Clamp(FallStoryPage, 0, FMath::Max(0, (PendingFalls.Num() - 1) / FallsPerPage));
        for (int32 Slot = 0; Slot < FallsPerPage; ++Slot)
        {
            const int32 Row = FallStoryPage * FallsPerPage + Slot;
            if (!PendingFalls.IsValidIndex(Row)) { break; }
            const int32 Index = PendingFalls[Row];
            if (Index == INDEX_NONE)
            {
                Button(BirdStory, bBirdGlassKnown || BirdReports == 1
                    ? TEXT("Bird Strike at Rivermere Mall") : TEXT("Injured Bird at Rivermere Mall"), 232, 396 + Slot * 38, 335);
                continue;
            }
            const bool bWallet = Index >= SelectWalletStory;
            const FString Label = bWallet ? FString::Printf(TEXT("Wallet at the Park (Day %d)"),
                LostCases[Index - SelectWalletStory].Day) : FallStoryLabel(Index);
            Button(bWallet ? Index : SelectFallStory + Index, Label, 232, 396 + Slot * 38, 335);
        }
        if (FallStoryPage > 0) { Button(FallListPrevious, TEXT("<"), 232, 520, 36); }
        if ((FallStoryPage + 1) * FallsPerPage < PendingFalls.Num()) { Button(FallListNext, TEXT(">"), 530, 520, 36); }
        if (PendingFalls.IsEmpty() && !HasPendingFountainStory())
        {
            Text(TEXT("No pending events"), 232, 329);
        }
        return;
    }
    Button(ChooseStory, TEXT("Discard Draft"), 646, 51, 140);
    const TCHAR* AngleLabel = (StoryKind == 2 || StoryKind == 4) && StoryAngle == 1 ? TEXT("Verified") : Angles[StoryAngle];
    const TCHAR* const Tabs[] = {TEXT("Story"), TEXT("Photo"), TEXT("Front Page")};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Tabs); ++Index)
    {
        Button(Tab + Index, Tabs[Index], 16 + 118 * Index, 51, 108, EditorTab == Index);
    }
    Rect(14, 90, 178, 487, FLinearColor(0.78f, 0.71f, 0.55f));
    Text(TEXT("Photo Library"), 24, 102);
    if (EditorPhotos.IsEmpty())
    {
        Text(TEXT("No photos yet."), 24, 142);
        Text(TEXT("Take a photo outside."), 24, 166, Ink, 0.82f);
    }
    for (int32 Slot = 0; Slot < 3; ++Slot)
    {
        const int32 Index = PhotoPage * 3 + Slot;
        if (!EditorPhotos.IsValidIndex(Index)) { break; }
        const int32 Y = 139 + Slot * 111;
        Rect(23, Y, 160, 79, Index == PhotoIndex ? Coral : Teal);
        Photo(Index, 26, Y + 3, 154, 73);
        EditorActions.Emplace(FBox2D(Origin + FVector2D(23, Y) * S,
            Origin + FVector2D(183, Y + 79) * S), SelectPhoto + Index);
        Text(EditorPhotoNames[Index],
            26, Y + 82, Ink, 0.78f);
    }
    Button(PreviousPhotos, TEXT("<"), 24, 478, 35);
    Button(NextPhotos, TEXT(">"), 146, 478, 35);
    Text(FString::Printf(TEXT("%d / %d"), PhotoPage + 1, FMath::Max(1, (EditorPhotos.Num() + 2) / 3)), 80, 484, Ink, 0.8f);
    Button(AddPhoto, TEXT("Add to Story"), 24, 514, 156);
    Button(RemovePhoto, TEXT("Remove from Story"), 24, 545, 156);

    if (EditorTab == 1)
    {
        Rect(210, 96, 576, 368, Ink);
        Photo(PhotoIndex, 219, 103, 558, 354, false, true);
        if (CropCanvas.bIsValid)
        {
            UpdateCrop();
            const FVector4 Selection = WorkingPhoto.Crop;
            const FVector2D A = CropCanvas.Min + FVector2D(Selection.X, Selection.Y) * CropCanvas.GetSize();
            const FVector2D B = CropCanvas.Min + FVector2D(Selection.Z, Selection.W) * CropCanvas.GetSize();
            const FLinearColor Shade(0, 0, 0, 0.48f);
            DrawRect(Shade, CropCanvas.Min.X, CropCanvas.Min.Y, CropCanvas.GetSize().X, A.Y - CropCanvas.Min.Y);
            DrawRect(Shade, CropCanvas.Min.X, B.Y, CropCanvas.GetSize().X, CropCanvas.Max.Y - B.Y);
            DrawRect(Shade, CropCanvas.Min.X, A.Y, A.X - CropCanvas.Min.X, B.Y - A.Y);
            DrawRect(Shade, B.X, A.Y, CropCanvas.Max.X - B.X, B.Y - A.Y);
            for (double X : {A.X, B.X})
            {
                DrawLine(X, A.Y, X, B.Y, FLinearColor::White, 2);
                for (double Y : {A.Y, B.Y})
                {
                    DrawRect(FLinearColor::White, X - 3, Y - 3, 6, 6);
                }
            }
            DrawLine(A.X, A.Y, B.X, A.Y, FLinearColor::White, 2);
            DrawLine(A.X, B.Y, B.X, B.Y, FLinearColor::White, 2);
        }
        Button(CropPhoto, CropRatio == 0 ? TEXT("Crop: Free") : CropRatio == 1 ? TEXT("Crop: 4:3") : TEXT("Crop: 16:9"), 210, 479, 120);
        Button(RotatePhoto, TEXT("Rotate 90"), 337, 479, 120);
        Button(FilterPhoto, TEXT("Filter: cycle"), 465, 479, 120);
        Button(ResetPhoto, TEXT("Reset Photo"), 594, 479, 136);
        if (EditorPhotoEdits.IsValidIndex(PhotoIndex))
        {
            const FEditorPhoto& Edit = WorkingPhoto;
            const TCHAR* const FilterNames[] = {TEXT("Original"), TEXT("Warm"), TEXT("Cool"), TEXT("Muted")};
            Text(FString::Printf(TEXT("Drag to crop  |  %d degrees  |  %s"), Edit.Turn * 90, FilterNames[Edit.Filter]), 213, 515);
            Button(ApplyDraftPhoto, TEXT("Apply to Draft"), 210, 547, 158, true);
            Button(CopyPhoto, TEXT("Save as Copy"), 379, 547, 158);
        }
        return;
    }

    if (EditorTab == 0)
    {
        if (StoryKind == 0)
        {
            Text(TEXT("Free Story"), 209, 100);
        }
        else
        {
        Text(TEXT("Reporting Angle"), 209, 100);
        if (StoryKind == 4) { Text(AngleLabel, 209, 125, Teal); }
        else { Button(NextAngle, AngleLabel, 209, 125, 122); }
        if (StoryKind >= 2)
        {
            const bool bVerified = StoryKind == 2 ? HasVerifiedFallCause(FallCases[StoryFallCase])
                : StoryKind == 4 ? StoryAngle == 1 : LostCases[StoryWalletCase].bAskedFinder;
            Text(bVerified ? TEXT("Account verified") : TEXT("Account unconfirmed"), 209, 164, Teal, 0.8f);
            DrawStoryParagraphs(DraftHeadline, Origin + FVector2D(209, 222) * S, 251 * S, 0.92f * S, 2);
            DrawStoryParagraphs(DraftBody, Origin + FVector2D(209, 303) * S, 251 * S, 0.90f * S, 10);
        }
        else
        {
            Button(NextTemplate, FString::Printf(TEXT("Template %d / 2"), StoryTemplate + 1), 338, 125, 129);
        }
        if (StoryKind < 2) { Button(ApplyTemplate, TEXT("Apply Template"), 209, 161, 145); }
        if (StoryKind == 1 && bFirstStoryGuide)
        {
            const TCHAR* AngleHints[] = {
                TEXT("Report what you found."),
                TEXT("Leave the reader guessing."),
                TEXT("Turn the toy into a water monster.")
            };
            Text(AngleHints[StoryAngle], 209, 185, Teal, 0.75f);
        }
        }
        Text(TEXT("Headline"), 209, 200);
        Text(TEXT("Body"), 209, 275);
    }
    const double PageX = EditorTab == 0 ? 480 : 210;
    const double PageW = EditorTab == 0 ? 307 : 378;
    const bool bPrint = EditorTab == 2;
    const int32 Pages = DrawNewspaper(GetDraftNewspaper(), Origin + FVector2D(PageX, 96) * S,
        FVector2D(PageW, 406) * S, S, bPrint, BodyPage);
    if (Pages > 1)
    {
        Button(PreviousBody, TEXT("<"), PageX + PageW - 120, 506, 30);
        Text(FString::Printf(TEXT("%d/%d"), BodyPage + 1, Pages), PageX + PageW - 83, 511, Ink, 0.8f);
        Button(NextBody, TEXT(">"), PageX + PageW - 36, 506, 30);
    }

    for (int32 Index = 0; Index < ArticlePhotos.Num(); ++Index)
    {
        Button(SelectArticlePhoto + Index, Index == 0 ? TEXT("Lead") : FString::FromInt(Index + 1),
            210 + Index * 61, 515, 56, PhotoIndex == ArticlePhotos[Index]);
    }
    Button(MakeLead, TEXT("Set as Lead"), 394, 515, 108);
    if (EditorTab == 0)
    {
        Button(Tab + 2, TEXT("Review Draft"), 648, 545, 138, true);
    }
    else
    {
        Button(NextLayout, ArticleLayout ? TEXT("Text First") : TEXT("Lead Photo"), 603, 103, 183);
        Button(NextSize, FString::Printf(TEXT("Title Size: %s"), HeadlineSize == 0 ? TEXT("Small") : HeadlineSize == 1 ? TEXT("Medium") : TEXT("Large")), 603, 142, 183);
        Button(ToggleBold, TEXT("Bold"), 603, 181, 87, bHeadlineBold);
        Button(ToggleUnderline, TEXT("Underline"), 697, 181, 89, bUnderline);
        Button(NextPaper, Paper ? TEXT("Paper: Warm Ivory") : TEXT("Paper: White"), 603, 220, 183);
        Button(NextStamp, Stamp == 0 ? TEXT("Stamp: None") : Stamp == 1 ? TEXT("Stamp: Local") : TEXT("Stamp: Correction"), 603, 259, 183);
        Button(NextBodySize, FString::Printf(TEXT("Body: %s"),
            BodySize == 0 ? TEXT("Small") : BodySize == 1 ? TEXT("Medium") : TEXT("Large")), 603, 298, 183);
        Text(StoryKind == 0 ? FString(TEXT("Free Story")) : FString::Printf(TEXT("Angle: %s"), AngleLabel), 603, 342, Ink, 0.9f);
        if (!DraftHeadline.TrimStartAndEnd().IsEmpty() && !ArticlePhotos.IsEmpty() &&
            CanPublishFallStory() && CanPublishWalletStory() && CanPublishBirdStory())
        {
            if (!bDraftCostPaid)
            {
                const int32 StaminaCost = FurnitureCost(EFurniture::Desk, DraftStamina);
                Text(ReporterStamina >= StaminaCost
                    ? FString::Printf(TEXT("%s  |  %d Stamina"),
                        *FurnitureDuration(FurnitureCost(EFurniture::Computer, DraftMinutes)), StaminaCost)
                    : FString(TEXT("Rest to restore Stamina")), 603, 478, Ink, 0.87f);
            }
            Button(SaveFrontPage, bFrontPageSaved ? TEXT("Front Page Saved") : TEXT("Save Front Page"), 603, 508, 183);
            if (bDraftCostPaid)
            {
                Button(PublishArticle, TEXT("Publish"), 603, 545, 183, true);
            }
        }
        else
        {
            Text(StoryKind >= 2 ? TEXT("Add a scene photo") : TEXT("Add a title and photo"), 603, 550, Ink, 0.85f);
        }
    }
}

bool AReporterHUD::ClickStoryEditor(FVector2D Mouse)
{
    if (EditorTab == 1 && CropCanvas.bIsValid && CropCanvas.IsInsideOrOn(Mouse))
    {
        CropStart = (Mouse - CropCanvas.Min) / CropCanvas.GetSize();
        CropAtGrab = WorkingPhoto.Crop;
        CropDrag = 1;
        const bool bCropped = CropAtGrab.X > 0 || CropAtGrab.Y > 0 || CropAtGrab.Z < 1 || CropAtGrab.W < 1;
        if (bCropped)
        {
            for (int32 Corner = 0; Corner < 4; ++Corner)
            {
                const FVector2D At(Corner & 1 ? CropAtGrab.Z : CropAtGrab.X,
                    Corner & 2 ? CropAtGrab.W : CropAtGrab.Y);
                if (((At - CropStart) * CropCanvas.GetSize()).SizeSquared() < 144)
                {
                    CropStart = FVector2D(Corner & 1 ? CropAtGrab.X : CropAtGrab.Z,
                        Corner & 2 ? CropAtGrab.Y : CropAtGrab.W);
                    return true;
                }
            }
            if (CropStart.X > CropAtGrab.X && CropStart.X < CropAtGrab.Z &&
                CropStart.Y > CropAtGrab.Y && CropStart.Y < CropAtGrab.W)
            {
                CropDrag = 2;
            }
        }
        return true;
    }
    for (const auto& Entry : EditorActions)
    {
        if (!Entry.Key.IsInsideOrOn(Mouse)) { continue; }
        const int32 Action = Entry.Value;
        if (Action >= SelectWalletStory)
        {
            const int32 Index = Action - SelectWalletStory;
            if (!LostCases.IsValidIndex(Index) || LostCases[Index].bPublished ||
                !EditorPhotos.IsValidIndex(LostCases[Index].LatestPhoto)) { return true; }
            StoryKind = 3;
            StoryWalletCase = Index;
            StoryAngle = 0;
            EditorTab = BodyPage = 0;
            bDraftCostPaid = bFrontPageSaved = false;
            const int32 Photo = LostCases[Index].LatestPhoto;
            SelectEditorPhoto(Photo);
            PhotoPage = Photo / 3;
            ArticlePhotos = {Photo};
            ApplyWalletTemplate();
        }
        else if (Action >= SelectFallStory)
        {
            const int32 Index = Action - SelectFallStory;
            if (!FallCases.IsValidIndex(Index) || FallCases[Index].bPublished ||
                !EditorPhotos.IsValidIndex(FallCases[Index].LatestPhoto)) { return true; }
            StoryKind = 2;
            StoryFallCase = Index;
            StoryAngle = 0;
            EditorTab = BodyPage = 0;
            bDraftCostPaid = bFrontPageSaved = false;
            const int32 Photo = FallCases[Index].LatestPhoto;
            SelectEditorPhoto(Photo);
            PhotoPage = Photo / 3;
            ArticlePhotos = {Photo};
            ApplyFallTemplate();
        }
        else if (Action >= SelectArticlePhoto)
        {
            SelectEditorPhoto(ArticlePhotos[Action - SelectArticlePhoto]);
        }
        else if (Action >= SelectPhoto)
        {
            SelectEditorPhoto(Action - SelectPhoto);
        }
        else if (Action >= Tab && Action < PreviousPhotos)
        {
            EditorTab = Action - Tab;
            BodyPage = 0;
            if (EditorTab != 0) { HideStoryInputs(); }
        }
        else
        {
            if (Action != SaveFrontPage) { bFrontPageSaved = false; }
            switch (Action)
            {
            case BirdStory:
                if (const int32 Photo = GetBirdPhotoIndex(); EditorPhotos.IsValidIndex(Photo))
                {
                    StoryKind = 4;
                    EditorTab = BodyPage = 0;
                    bDraftCostPaid = bFrontPageSaved = bCustomStory = false;
                    SelectEditorPhoto(Photo);
                    PhotoPage = Photo / 3;
                    ArticlePhotos = {Photo};
                    ApplyBirdTemplate();
                }
                break;
            case FreeStory:
            case EventStory:
            {
                if (Action == EventStory && !HasPendingFountainStory())
                {
                    break;
                }
                bDraftCostPaid = false;
                StoryKind = Action == EventStory ? 1 : 0;
                StoryAngle = 0;
                StoryTemplate = 0;
                EditorTab = 0;
                BodyPage = 0;
                ArticlePhotos.Reset();
                if (StoryKind == 1)
                {
                    const int32 EventPhoto = LatestFountainPhotoIndex;
                    SelectEditorPhoto(EventPhoto);
                    PhotoPage = EventPhoto / 3;
                    ArticlePhotos.Add(EventPhoto);
                    ApplyStoryTemplate();
                }
                else
                {
                    SelectEditorPhoto(FMath::Max(0, EditorPhotos.Num() - 1));
                    PhotoPage = PhotoIndex / 3;
                    DraftHeadline.Reset();
                    DraftBody.Reset();
                    bCustomStory = false;
                }
                break;
            }
            case ChooseStory:
                StoryKind = INDEX_NONE;
                StoryFallCase = INDEX_NONE;
                HideStoryInputs();
                break;
            case FallListPrevious: --FallStoryPage; break;
            case FallListNext: ++FallStoryPage; break;
            case PreviousPhotos: PhotoPage = FMath::Max(0, PhotoPage - 1); break;
            case NextPhotos: PhotoPage = FMath::Min(PhotoPage + 1, FMath::Max(0, (EditorPhotos.Num() - 1) / 3)); break;
            case AddPhoto:
                if (EditorPhotos.IsValidIndex(PhotoIndex) && ArticlePhotos.Num() < 3) { ArticlePhotos.AddUnique(PhotoIndex); }
                break;
            case RemovePhoto: ArticlePhotos.Remove(PhotoIndex); break;
            case MakeLead:
                if (ArticlePhotos.Contains(PhotoIndex))
                {
                    ArticlePhotos.Remove(PhotoIndex);
                    ArticlePhotos.Insert(PhotoIndex, 0);
                }
                break;
            case NextAngle:
                StoryAngle = (StoryAngle + 1) % 3;
                if (StoryKind == 2)
                {
                    const FFallCase& Incident = FallCases[StoryFallCase];
                    if (StoryAngle == 1 && !HasVerifiedFallCause(Incident)) { StoryAngle = 2; }
                    ApplyFallTemplate();
                }
                else if (StoryKind == 3) { ApplyWalletTemplate(); }
                else if (!bCustomStory) { ApplyStoryTemplate(); }
                break;
            case ApplyTemplate: ApplyStoryTemplate(); break;
            case NextTemplate:
                StoryTemplate = 1 - StoryTemplate;
                if (!bCustomStory) { ApplyStoryTemplate(); }
                break;
            case NextBodySize: BodySize = (BodySize + 1) % 3; BodyPage = 0; break;
            case PreviousBody: BodyPage = FMath::Max(0, BodyPage - 1); break;
            case NextBody: ++BodyPage; break;
            case CropPhoto: CropRatio = (CropRatio + 1) % 3; break;
            case ApplyDraftPhoto:
                if (DraftPhotoEdits.IsValidIndex(PhotoIndex)) { DraftPhotoEdits[PhotoIndex] = WorkingPhoto; }
                break;
            case CopyPhoto:
                if (EditorPhotos.IsValidIndex(PhotoIndex) && EditorPhotos.Num() < 100)
                {
                    const TObjectPtr<UTexture2D> Texture = EditorPhotos[PhotoIndex];
                    const bool bEventCopy = FountainPhotos.Contains(PhotoIndex);
                    const int32* FallCase = FallPhotoCases.Find(PhotoIndex);
                    const int32* WalletCase = WalletPhotoCases.Find(PhotoIndex);
                    EditorPhotos.Add(Texture);
                    const FString SourceFile = EditorPhotoFiles[PhotoIndex];
                    EditorPhotoFiles.Add(SourceFile);
                    if (bEventCopy) { FountainPhotos.Add(EditorPhotos.Num() - 1); }
                    if (FallCase)
                    {
                        const int32 CaseIndex = *FallCase;
                        FallPhotoCases.Add(EditorPhotos.Num() - 1, CaseIndex);
                    }
                    if (WalletCase)
                    {
                        const int32 Index = *WalletCase;
                        WalletPhotoCases.Add(EditorPhotos.Num() - 1, Index);
                    }
                    EditorPhotoEdits.Add(WorkingPhoto);
                    DraftPhotoEdits.Add(WorkingPhoto);
                    EditorPhotoNames.Add(FString::Printf(TEXT("Copy %d"), EditorPhotos.Num()));
                    SelectEditorPhoto(EditorPhotos.Num() - 1);
                    PhotoPage = PhotoIndex / 3;
                }
                break;
            case NextLayout: ArticleLayout = 1 - ArticleLayout; break;
            case NextSize: HeadlineSize = (HeadlineSize + 1) % 3; break;
            case ToggleBold: bHeadlineBold = !bHeadlineBold; break;
            case ToggleUnderline: bUnderline = !bUnderline; break;
            case NextPaper: Paper = 1 - Paper; break;
            case NextStamp: Stamp = (Stamp + 1) % 3; break;
            case SaveFrontPage:
                if (!bDraftCostPaid)
                {
                    const int32 StaminaCost = FurnitureCost(EFurniture::Desk, DraftStamina);
                    if (ReporterStamina < StaminaCost)
                    {
                        break;
                    }
                    ReporterStamina -= StaminaCost;
                    bDraftCostPaid = true;
                    bFrontPageSaved = true;
                    AdvanceGameMinutes(FurnitureCost(EFurniture::Computer, DraftMinutes));
                }
                bFrontPageSaved = true;
                break;
            case PublishArticle:
                if (!CanPublishFallStory() || !CanPublishWalletStory() || !CanPublishBirdStory()) { break; }
                PublishNewspaper();
                if (StoryKind == 2) { PublishFallStory(); }
                if (StoryKind == 3) { LostCases[StoryWalletCase].bPublished = true; }
                if (StoryKind == 4) { PublishBirdStory(); }
                if (StoryKind == 1)
                {
                    bFountainReported = true;
                    bShowFirstStoryResults = bFirstStoryGuide;
                    bFirstStoryGuide = false;
                }
                StoryKind = INDEX_NONE;
                DesktopPage = 2;
                break;
            default:
                if (EditorPhotoEdits.IsValidIndex(PhotoIndex))
                {
                    FEditorPhoto& Edit = WorkingPhoto;
                    if (Action == RotatePhoto) { Edit.Turn = (Edit.Turn + 1) % 4; }
                    if (Action == FilterPhoto) { Edit.Filter = (Edit.Filter + 1) % 4; }
                    if (Action == ResetPhoto) { Edit = FEditorPhoto(); }
                }
                break;
            }
        }
        return true;
    }
    return false;
}


void AReporterHUD::SelectEditorPhoto(int32 Index)
{
    if (EditorPhotoEdits.IsValidIndex(Index))
    {
        PhotoIndex = Index;
        WorkingPhoto = DraftPhotoEdits[Index];
        CropDrag = 0;
    }
}

void AReporterHUD::UpdateCrop()
{
    float X, Y;
    if (!CropDrag || !CropCanvas.bIsValid || !GetOwningPlayerController()->GetMousePosition(X, Y))
    {
        return;
    }
    FVector2D Point = (FVector2D(X, Y) - CropCanvas.Min) / CropCanvas.GetSize();
    Point.X = FMath::Clamp(Point.X, 0.0, 1.0);
    Point.Y = FMath::Clamp(Point.Y, 0.0, 1.0);
    if (CropDrag == 2)
    {
        const double DX = FMath::Clamp(Point.X - CropStart.X, -CropAtGrab.X, 1.0 - CropAtGrab.Z);
        const double DY = FMath::Clamp(Point.Y - CropStart.Y, -CropAtGrab.Y, 1.0 - CropAtGrab.W);
        WorkingPhoto.Crop = FVector4(CropAtGrab.X + DX, CropAtGrab.Y + DY,
            CropAtGrab.Z + DX, CropAtGrab.W + DY);
        return;
    }
    if (CropRatio != 0)
    {
        const double Ratio = (CropRatio == 1 ? 4.0 / 3.0 : 16.0 / 9.0)
            * CropCanvas.GetSize().Y / CropCanvas.GetSize().X;
        FVector2D Delta = Point - CropStart;
        const double Width = FMath::Min(FMath::Abs(Delta.X), FMath::Abs(Delta.Y) * Ratio);
        Delta.X = FMath::Sign(Delta.X) * Width;
        Delta.Y = FMath::Sign(Delta.Y) * Width / Ratio;
        Point = CropStart + Delta;
    }
    if (FMath::Abs(Point.X - CropStart.X) >= 0.015 && FMath::Abs(Point.Y - CropStart.Y) >= 0.015)
    {
        WorkingPhoto.Crop = FVector4(FMath::Min(Point.X, CropStart.X), FMath::Min(Point.Y, CropStart.Y),
            FMath::Max(Point.X, CropStart.X), FMath::Max(Point.Y, CropStart.Y));
    }
}

void AReporterHUD::ApplyStoryTemplate()
{
    DraftHeadline = Headlines[StoryAngle][StoryTemplate];
    DraftBody = Bodies[StoryAngle];
    bCustomStory = false;
    BodyPage = 0;
    HideStoryInputs();
}

void AReporterHUD::ShowStoryInputs(FVector2D Origin, double Scale)
{
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    if (!Viewport || Canvas->ClipX <= 0 || Canvas->ClipY <= 0)
    {
        return;
    }
    const FVector2D CanvasSize(Canvas->ClipX, Canvas->ClipY);
    if (StoryInputs && StoryInputOrigin.Equals(Origin) &&
        StoryInputCanvasSize.Equals(CanvasSize) && FMath::IsNearlyEqual(StoryInputScale, Scale))
    {
        return;
    }
    HideStoryInputs();
    StoryInputOrigin = Origin;
    StoryInputScale = Scale;
    StoryInputCanvasSize = CanvasSize;
    const auto Anchors = [&](double Y, double Height)
    {
        const FVector2D Min = (Origin + FVector2D(209, Y) * Scale) / CanvasSize;
        const FVector2D Max = (Origin + FVector2D(464, Y + Height) * Scale) / CanvasSize;
        return FAnchors(Min.X, Min.Y, Max.X, Max.Y);
    };
    static const FEditableTextBoxStyle FieldStyle = []()
    {
        FEditableTextBoxStyle Style = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
        Style.SetBackgroundImageNormal(FSlateColorBrush(FLinearColor(0.96f, 0.89f, 0.74f)))
            .SetBackgroundImageHovered(FSlateColorBrush(FLinearColor(1.0f, 0.95f, 0.84f)))
            .SetBackgroundImageFocused(FSlateColorBrush(FLinearColor(1.0f, 0.97f, 0.89f)))
            .SetForegroundColor(FSlateColor(Ink))
            .SetFocusedForegroundColor(FSlateColor(Ink))
            .SetPadding(FMargin(8, 6));
        return Style;
    }();
    TSharedRef<SConstraintCanvas> Fields = SNew(SConstraintCanvas)
        + SConstraintCanvas::Slot().Anchors(Anchors(225, 36)).Offset(FMargin(0))
        [
            SNew(SEditableTextBox).Style(&FieldStyle).Text(FText::FromString(DraftHeadline))
                .OnTextChanged_Lambda([this](const FText& Text)
                {
                    DraftHeadline = Text.ToString();
                    bCustomStory = true;
                    bFrontPageSaved = false;
                })
        ]
        + SConstraintCanvas::Slot().Anchors(Anchors(300, 176)).Offset(FMargin(0))
        [
            SNew(SMultiLineEditableTextBox).Style(&FieldStyle).Text(FText::FromString(DraftBody))
                .AutoWrapText(true).OnTextChanged_Lambda([this](const FText& Text)
                {
                    DraftBody = Text.ToString();
                    bCustomStory = true;
                    BodyPage = 0;
                    bFrontPageSaved = false;
                })
        ];
    Fields->SetVisibility(EVisibility::SelfHitTestInvisible);
    StoryInputs = Fields;
    Viewport->AddViewportWidgetContent(Fields, 50);
}

void AReporterHUD::HideStoryInputs()
{
    if (StoryInputs)
    {
        if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
        {
            Viewport->RemoveViewportWidgetContent(StoryInputs.ToSharedRef());
        }
        StoryInputs.Reset();
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
}

void AReporterHUD::EndPlay(const EEndPlayReason::Type Reason)
{
    HideStoryInputs();
    RemoveReporterCreationControls();
    RemoveTextLayer();
    StopBackgroundMusic();
    StopTreadmillRun();
    StopSwimRace();
    Super::EndPlay(Reason);
}
