#include "ReporterGameMode.h"
#include "ReporterSaveSlot.h"

#include "Components/AudioComponent.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include <type_traits>

namespace
{
constexpr int32 SaveVersion = 7;
constexpr int32 OldestSaveVersion = 1;
constexpr int32 MaxSaveSize = 192 * 1024 * 1024;

FString SlotName(int32 Slot)
{
    return FString::Printf(TEXT("ReporterSlot%d"), Slot + 1);
}

// The field order is the versioned on-disk contract. Append only with a version change.
#define REPORTER_SAVE_FIELDS(X) \
    X(ReporterName) X(ReporterTalent) X(bFemaleReporter) X(ReporterRollsLeft) \
    X(OutdoorGameMinutes) X(ReporterCash) X(ReporterFullness) X(ReporterStamina) X(ReporterMorality) \
    X(bInLobby) X(bInEditorial) X(bInSportsCenter) X(bInHotPot) X(bInGym) X(bInPool) X(bUpstairs) \
    X(LobbyX) X(EditorialPosition) X(PoolPosition) X(IndoorDirection) \
    X(bHasDesk) X(DeskPosition) X(DeskTurn) X(DeskScale) X(ComputerPosition) X(ComputerScale) \
    X(bHasBed) X(BedPosition) X(BedTurn) X(BedScale) X(BedAngle) X(bDecorating) X(bBedSelected) \
    X(bSponsorshipClaimed) X(MailPage) X(SelectedMail) X(bToyOwnerMailRead) \
    X(bNewsroomSetupGuide) X(bFirstStoryGuide) X(bShowFirstStoryResults) \
    X(bFountainDiscovered) X(bFountainReported) X(LatestCapturedFountainPhoto) \
    X(LatestImportedFountainPhoto) X(LatestFountainPhotoIndex) X(ToyPlace) X(ToyReplyDay) X(bToyReturned) \
    X(FoundCoins) X(FoundLitter) X(LastFindDay) X(CoinsValue) X(bPennyAchievement) X(bBackpackOpen) \
    X(LostCases) X(WalletPhotoFiles) X(WalletPhotoCases) X(LastLostRollDay) X(NextLostDay) X(StoryWalletCase) \
    X(FallCases) X(FallPhotoFiles) X(FallPhotoCases) X(ActiveFallCase) X(StoryFallCase) \
    X(FallStoryPage) X(LastFallRollDay) X(NextFallDay) \
    X(CapturedPhotoFiles) X(EditorPhotoFiles) X(EditorPhotoNames) X(FountainPhotos) \
    X(EditorPhotoEdits) X(DraftPhotoEdits) X(WorkingPhoto) X(CropRatio) \
    X(DraftHeadline) X(DraftBody) X(StoryKind) X(StoryAngle) X(StoryTemplate) X(bCustomStory) \
    X(EditorTab) X(PhotoIndex) X(PhotoPage) X(ArticlePhotos) X(ArticleLayout) X(HeadlineSize) \
    X(BodySize) X(BodyPage) X(Paper) X(Stamp) X(bHeadlineBold) X(bUnderline) X(bFrontPageSaved) X(bDraftCostPaid) \
    X(PublishedArticles) X(DailyArticleRevenue) X(NewsroomFollowers) X(SelectedPublishedArticle) \
    X(NewsroomListPage) X(NewsroomBodyPage) X(bNewsroomOverview) \
    X(bDesktopOpen) X(DesktopPage) X(StreetPedestrians) X(PedestrianDay) X(bRecheckPedestriansOnStreet)
}

struct FReporterSaveState
{
#define DECLARE(Name) decltype(AReporterHUD::Name) Name{};
    REPORTER_SAVE_FIELDS(DECLARE)
#undef DECLARE

    FVector Position = FVector::ZeroVector;
    EReporterFacing Facing = EReporterFacing::Front;
    int32 Skills[3] = {};
    bool OpeningMailRead[2] = {};
    int32 RepairDays[2] = {};
    TMap<FString, TArray<uint8>> PhotoBytes;
    AReporterHUD::FEndingIssue EndingIssue;
    int32 ExhaustedNights = 0;
    int32 ReporterHealth = AReporterHUD::MaxHealth;
    bool bInHospital = false;
    int32 BirdReports = 0;
    int32 BirdNextDay = 5;
    int32 BirdFixDay = 0;
    int32 BirdSeenRound = 0;
    bool bBirdGlassKnown = false;
    bool bNewsroomComments = false;
    FIntVector OwnedFurniture = FIntVector::ZeroValue;
    FIntVector FoodStock = FIntVector::ZeroValue;
    FIntVector EquippedFurniture = FIntVector::ZeroValue;
    bool bInMall = false;
    FVector2D MallPosition = FVector2D(0.5, 0.92);

    FReporterSaveState() = default;

    FReporterSaveState(const AReporterHUD& Hud, const AReporterPawn& Pawn)
    {
#define SNAPSHOT(Name) Name = Hud.Name;
        REPORTER_SAVE_FIELDS(SNAPSHOT)
#undef SNAPSHOT
        EndingIssue = Hud.EndingIssue;
        ExhaustedNights = Hud.ExhaustedNights;
        ReporterHealth = Hud.ReporterHealth;
        bInHospital = Hud.bInHospital;
        BirdReports = Hud.BirdReports;
        BirdNextDay = Hud.BirdNextDay;
        BirdFixDay = Hud.BirdFixDay;
        BirdSeenRound = Hud.BirdSeenRound;
        bBirdGlassKnown = Hud.bBirdGlassKnown;
        bNewsroomComments = Hud.bNewsroomComments;
        OwnedFurniture = Hud.OwnedFurniture;
        FoodStock = Hud.FoodStock;
        EquippedFurniture = Hud.EquippedFurniture;
        bInMall = Hud.bInMall;
        MallPosition = Hud.MallPosition;
        Position = Pawn.GetActorLocation();
        Facing = Pawn.Facing;
        for (int32 Index = 0; Index < 3; ++Index) { Skills[Index] = Hud.ReporterSkills[Index]; }
        for (int32 Index = 0; Index < 2; ++Index)
        {
            OpeningMailRead[Index] = Hud.bMailRead[Index];
            RepairDays[Index] = Hud.FallRepairDays[Index];
        }
    }

    void Apply(AReporterHUD& Hud, AReporterPawn& Pawn)
    {
#define RESTORE(Name) Hud.Name = MoveTemp(Name);
        REPORTER_SAVE_FIELDS(RESTORE)
#undef RESTORE
        Hud.EndingIssue = MoveTemp(EndingIssue);
        Hud.ExhaustedNights = ExhaustedNights;
        Hud.ReporterHealth = ReporterHealth;
        Hud.bInHospital = bInHospital;
        Hud.BirdReports = BirdReports;
        Hud.BirdNextDay = BirdNextDay;
        Hud.BirdFixDay = BirdFixDay;
        Hud.BirdSeenRound = BirdSeenRound;
        Hud.bBirdGlassKnown = bBirdGlassKnown;
        Hud.bNewsroomComments = bNewsroomComments;
        Hud.OwnedFurniture = OwnedFurniture;
        Hud.FoodStock = FoodStock;
        Hud.EquippedFurniture = EquippedFurniture;
        Hud.bInMall = bInMall;
        Hud.MallPosition = MallPosition;
        Pawn.SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
        Pawn.Facing = Facing;
        Pawn.Input = FVector2D::ZeroVector;
        Pawn.bWalking = false;
        Pawn.WalkProgress = 0;
        for (int32 Index = 0; Index < 3; ++Index) { Hud.ReporterSkills[Index] = Skills[Index]; }
        for (int32 Index = 0; Index < 2; ++Index)
        {
            Hud.bMailRead[Index] = OpeningMailRead[Index];
            Hud.FallRepairDays[Index] = RepairDays[Index];
        }
        Hud.RestoredPhotoBytes = MoveTemp(PhotoBytes);
    }

    template<typename T>
    static void Field(FArchive& Ar, T& Value)
    {
        if constexpr (std::is_enum_v<T>)
        {
            uint8 Encoded = static_cast<uint8>(Value);
            Ar << Encoded;
            if (Ar.IsLoading()) { Value = static_cast<T>(Encoded); }
        }
        else
        {
            Ar << Value;
        }
    }

    template<typename... T>
    static void Fields(FArchive& Ar, T&... Values)
    {
        (Field(Ar, Values), ...);
    }

    template<typename T>
    static void Field(FArchive& Ar, TArray<T>& Items)
    {
        int32 Count = Items.Num();
        Ar << Count;
        if (Ar.IsError() || Count < 0 || Count > 50000)
        {
            Ar.SetError();
            return;
        }
        if (Ar.IsLoading()) { Items.SetNum(Count); }
        for (T& Item : Items)
        {
            if (Ar.IsError()) { break; }
            Field(Ar, Item);
        }
    }

    static void Field(FArchive& Ar, AReporterHUD::FEditorPhoto& P)
    {
        Fields(Ar, P.Crop, P.Turn, P.Filter);
    }

    static void Field(FArchive& Ar, AReporterHUD::FStreetFind& I)
    {
        Fields(Ar, I.Place, I.Ground);
    }

    static void Field(FArchive& Ar, AReporterHUD::FLostCase& C)
    {
        Fields(Ar, C.Item, C.Place, C.ItemGround, C.SearchGround, C.Day, C.Owner, C.Finder,
            C.ReplyDay, C.LatestPhoto, C.bAskedFinder, C.bTalkedOwner, C.bPhotographed, C.bMailRead,
            C.bReturned, C.bPublished);
    }

    static void Field(FArchive& Ar, AReporterHUD::FFallCase& C)
    {
        Fields(Ar, C.Day, C.Site, C.Resident, C.Witness, C.Ground, C.Cause, C.Crowd, C.bDiscovered,
            C.bPanelOpen, C.bSpoke, C.bCheckedGround, C.bPublished, C.LatestPhoto);
    }

    static void Field(FArchive& Ar, AReporterHUD::FNewspaper& P)
    {
        Fields(Ar, P.Headline, P.Body, P.Photos, P.Edits, P.Layout, P.TitleSize, P.BodySize,
            P.Paper, P.Stamp, P.bBold, P.bUnderline);
    }

    static void Field(FArchive& Ar, AReporterHUD::FPublishedArticle& A)
    {
        Fields(Ar, A.Page, A.Day, A.Views, A.Earnings, A.InitialViews, A.bExposed, A.Angle);
    }

    static void Field(FArchive& Ar, AReporterHUD::FPublishedArticle::FComment& Comment)
    {
        Fields(Ar, Comment.Reader, Comment.Body, Comment.Day);
    }

    static void Field(FArchive& Ar, AReporterHUD::FStreetPedestrian& P)
    {
        Fields(Ar, P.Character, P.Ground, P.Speech, P.WalkTime, P.SecondsUntilPause,
            P.PauseSeconds, P.SpeechSeconds, P.bWalkRight, P.bGreetingChecked);
    }

    void Serialize(FArchive& Ar, int32 Version)
    {
#define SERIALIZE(Name) Field(Ar, Name);
        REPORTER_SAVE_FIELDS(SERIALIZE)
#undef SERIALIZE
        Fields(Ar, Position, Facing, Skills[0], Skills[1], Skills[2],
            OpeningMailRead[0], OpeningMailRead[1], RepairDays[0], RepairDays[1]);
        Ar << PhotoBytes;
        if (Version >= 2)
        {
            Fields(Ar, EndingIssue.Kind, EndingIssue.Day, EndingIssue.Stories, EndingIssue.Captions, ExhaustedNights);
        }
        if (Version >= 3)
        {
            Field(Ar, bNewsroomComments);
            for (AReporterHUD::FPublishedArticle& Article : PublishedArticles)
            {
                Fields(Ar, Article.StoryType, Article.EventIndex, Article.bPrintedEventFollowup, Article.Comments);
                if (Ar.IsError() || Article.Comments.Num() > 4)
                {
                    Ar.SetError();
                    return;
                }
            }
        }
        else if (Ar.IsLoading() && !Ar.IsError())
        {
            for (int32 Index = 0; Index < PublishedArticles.Num(); ++Index)
            {
                AReporterHUD::SeedReaderComments(PublishedArticles[Index], Index);
            }
        }
        if (Version >= 4)
        {
            Fields(Ar, OwnedFurniture, EquippedFurniture, bInMall, MallPosition);
        }
        if (Version >= 5)
        {
            Field(Ar, FoodStock);
            for (int32 Index = 0; Index < 3; ++Index)
            {
                if (FoodStock[Index] < 0 || FoodStock[Index] > AReporterHUD::MaxFoodStock) { Ar.SetError(); }
            }
        }
        if (Version >= 6)
        {
            Fields(Ar, ReporterHealth, bInHospital);
        }
        else if (Ar.IsLoading() && !Ar.IsError())
        {
            ReporterHealth = FMath::Clamp(AReporterHUD::MaxHealth - FMath::Clamp(ExhaustedNights, 0, 3) *
                AReporterHUD::ExhaustionHealthLoss, 0, AReporterHUD::MaxHealth);
        }
        if (Version >= 7)
        {
            Fields(Ar, BirdReports, BirdNextDay, BirdFixDay, BirdSeenRound, bBirdGlassKnown);
            if (BirdReports < 0 || BirdReports > 2 || BirdNextDay < 5 || BirdFixDay < 0 ||
                BirdSeenRound < 0 || BirdSeenRound > 2)
            {
                Ar.SetError();
            }
        }
    }

    bool HasValidFurniture() const
    {
        for (int32 Category = 0; Category < 3; ++Category)
        {
            const int32 Mask = OwnedFurniture[Category];
            const int32 Tier = EquippedFurniture[Category];
            if (Mask < 0 || Mask > 7 || Tier < 0 || Tier > 3 ||
                (Tier > 0 && (Mask & (1 << (Tier - 1))) == 0))
            {
                return false;
            }
        }
        return FMath::IsFinite(MallPosition.X) && FMath::IsFinite(MallPosition.Y) &&
            (!bInMall || (bInLobby && !bInEditorial && !bInSportsCenter && !bInHotPot));
    }

    bool HasValidEnding() const
    {
        using EKind = AReporterHUD::EEndingKind;
        if (ExhaustedNights < 0 || ExhaustedNights > 3) { return false; }
        const TArray<int32>& Stories = EndingIssue.Stories;
        if (Stories.Num() != EndingIssue.Captions.Num()) { return false; }
        switch (EndingIssue.Kind)
        {
        case EKind::None: return EndingIssue.Day == 0 && Stories.IsEmpty();
        case EKind::Special:
            return EndingIssue.Day == 30 && Stories.Num() == 4 &&
                Stories[0] >= 0 && Stories[0] <= 4 && Stories[1] >= 5 && Stories[1] <= 7 &&
                Stories[2] >= 8 && Stories[2] <= 10 && Stories[3] >= 11 && Stories[3] <= 13;
        case EKind::Hospital: return EndingIssue.Day > 0 && Stories.Num() == 1 && Stories[0] == 14;
        case EKind::Bankruptcy: return EndingIssue.Day > 0 && Stories.Num() == 1 && Stories[0] == 15;
        default: return false;
        }
    }

    bool HasValidPhotoIndices() const
    {
        if (EditorPhotoFiles.Num() != EditorPhotoEdits.Num() || EditorPhotoFiles.Num() != DraftPhotoEdits.Num() ||
            EditorPhotoFiles.Num() != EditorPhotoNames.Num())
        {
            return false;
        }
        const auto InvalidIndex = [&](int32 Index) { return !EditorPhotoFiles.IsValidIndex(Index); };
        if (ArticlePhotos.ContainsByPredicate(InvalidIndex) ||
            (StoryKind == 2 && !FallCases.IsValidIndex(StoryFallCase)) ||
            (StoryKind == 3 && !LostCases.IsValidIndex(StoryWalletCase)))
        {
            return false;
        }
        for (const AReporterHUD::FPublishedArticle& Article : PublishedArticles)
        {
            if (Article.Page.Photos.IsEmpty() || Article.Page.Photos.ContainsByPredicate(InvalidIndex) ||
                Article.Page.Photos.Num() != Article.Page.Edits.Num())
            {
                return false;
            }
        }
        return true;
    }
};

void AReporterHUD::RefreshSaveSlots()
{
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(SaveSlots); ++Index)
    {
        FSaveSlotPreview& Preview = SaveSlots[Index];
        Preview = FSaveSlotPreview();
        const FString Name = SlotName(Index);
        Preview.bOccupied = UGameplayStatics::DoesSaveGameExist(Name, 0);
        if (!Preview.bOccupied) { continue; }
        if (const UReporterSaveSlot* Save = Cast<UReporterSaveSlot>(UGameplayStatics::LoadGameFromSlot(Name, 0)))
        {
            Preview.Name = Save->ReporterName;
            Preview.Minutes = Save->GameMinutes;
            Preview.Location = Save->Location;
            Preview.bReadable = Save->Version >= OldestSaveVersion && Save->Version <= SaveVersion &&
                !Save->GameState.IsEmpty() &&
                Save->GameState.Num() <= MaxSaveSize && FMath::IsFinite(Save->GameMinutes) &&
                Save->GameMinutes >= 0 && Save->GameMinutes <= MAX_int32;
        }
    }
}

bool AReporterHUD::CanSaveAtCurrentLocation() const
{
    return !IsTitleOpen() && !bEndingOpen && GetOwningPawn() && PendingPhoto.IsEmpty() && StairProgress < 0 &&
        RunState == ERunState::Idle && SwimState == ESwimState::Idle && HotPotState == EHotPotState::Idle;
}

int32 AReporterHUD::GetSaveLocation() const
{
    if (bInHospital) { return 8; }
    if (bInMall) { return 7; }
    if (bInEditorial) { return 1; }
    if (bInGym) { return 2; }
    if (bInPool) { return 3; }
    if (bInHotPot) { return 4; }
    if (bInSportsCenter) { return 5; }
    return bInLobby ? 6 : 0;
}

bool AReporterHUD::SaveReporterGame(int32 Slot)
{
    const AReporterPawn* Pawn = Cast<AReporterPawn>(GetOwningPawn());
    if (!Pawn || !CanSaveAtCurrentLocation() || (Slot < 0 || Slot >= int32(UE_ARRAY_COUNT(SaveSlots))))
    {
        return false;
    }
    RefreshEditorPhotos();
    FReporterSaveState State(*this, *Pawn);
    if (!State.HasValidPhotoIndices()) { return false; }
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Photos");
    int64 MediaSize = 0;
    for (const FString& File : State.EditorPhotoFiles)
    {
        if (State.PhotoBytes.Contains(File)) { continue; }
        if (File.IsEmpty() || File != FPaths::GetCleanFilename(File)) { return false; }
        TArray<uint8>& Bytes = State.PhotoBytes.Add(File);
        if (const TArray<uint8>* Restored = RestoredPhotoBytes.Find(File))
        {
            Bytes = *Restored;
        }
        else if (!FFileHelper::LoadFileToArray(Bytes, *(Directory / File)))
        {
            return false;
        }
        MediaSize += Bytes.Num();
        if (Bytes.IsEmpty() || MediaSize > MaxSaveSize) { return false; }
    }
    // Only retain photos that were successfully imported into this snapshot.
    State.CapturedPhotoFiles.RemoveAll([&](const FString& File) { return !State.PhotoBytes.Contains(File); });
    UReporterSaveSlot* Save = NewObject<UReporterSaveSlot>();
    Save->Version = SaveVersion;
    Save->ReporterName = ReporterName;
    Save->GameMinutes = OutdoorGameMinutes;
    Save->Location = GetSaveLocation();
    FMemoryWriter Writer(Save->GameState, true);
    State.Serialize(Writer, SaveVersion);
    if (Writer.IsError() || Save->GameState.Num() > MaxSaveSize) { return false; }
    Save->Checksum = FCrc::MemCrc32(Save->GameState.GetData(), Save->GameState.Num());
    if (!UGameplayStatics::SaveGameToSlot(Save, SlotName(Slot), 0)) { return false; }
    RefreshSaveSlots();
    return true;
}

bool AReporterHUD::LoadReporterGame(int32 Slot)
{
    AReporterPawn* Pawn = Cast<AReporterPawn>(GetOwningPawn());
    if (!Pawn || !PendingPhoto.IsEmpty() || (Slot < 0 || Slot >= int32(UE_ARRAY_COUNT(SaveSlots)))) { return false; }
    const UReporterSaveSlot* Save = Cast<UReporterSaveSlot>(UGameplayStatics::LoadGameFromSlot(SlotName(Slot), 0));
    if (!Save || Save->Version < OldestSaveVersion || Save->Version > SaveVersion ||
        Save->GameState.IsEmpty() || Save->GameState.Num() > MaxSaveSize ||
        Save->Checksum != FCrc::MemCrc32(Save->GameState.GetData(), Save->GameState.Num()))
    {
        return false;
    }
    FReporterSaveState State;
    FMemoryReader Reader(Save->GameState, true);
    State.Serialize(Reader, Save->Version);
    if (Reader.IsError() || !Reader.AtEnd() || !State.HasValidPhotoIndices() || !State.HasValidEnding() ||
        !State.HasValidFurniture() || (State.bInMall && !MallInterior) ||
        State.ReporterHealth < 0 || State.ReporterHealth > MaxHealth ||
        (State.bInHospital && (!HospitalInterior || !State.bInLobby || State.bInMall ||
            State.bInEditorial || State.bInSportsCenter || State.bInHotPot)))
    {
        return false;
    }

    TArray<TObjectPtr<UTexture2D>> Photos;
    TMap<FString, UTexture2D*> Decoded;
    for (const FString& File : State.EditorPhotoFiles)
    {
        if (UTexture2D* const* Existing = Decoded.Find(File))
        {
            Photos.Add(*Existing);
            continue;
        }
        const TArray<uint8>* Bytes = State.PhotoBytes.Find(File);
        UTexture2D* Image = Bytes ? FImageUtils::ImportBufferAsTexture2D(*Bytes) : nullptr;
        if (!Image) { return false; }
        Decoded.Add(File, Image);
        Photos.Add(Image);
    }

    const bool bFromTitle = IsTitleOpen();
    CloseSettings();
    HideStoryInputs();
    RemoveReporterCreationControls();
    StopActivityMusic();
    RunState = ERunState::Idle;
    SwimState = ESwimState::Idle;
    HotPotState = EHotPotState::Idle;
    State.Apply(*this, *Pawn);
    EditorPhotos = MoveTemp(Photos);
    LoadedPhotoFiles.Reset();
    for (const FString& File : EditorPhotoFiles) { LoadedPhotoFiles.Add(File); }
    TitlePage = ETitlePage::Playing;
    bCameraMode = bShowGuide = bReporterPanelOpen = bIndoorWalking = false;
    bConfirmMorningRest = false;
    bFurnitureShopOpen = false;
    bFoodShopOpen = false;
    bDraggingDesk = bDraggingComputer = bDraggingBed = bRotatingBed = false;
    CropDrag = 0;
    StairProgress = -1;
    PendingFallPhotoCase = PendingLostPhoto = INDEX_NONE;
    HeldItem = EFoundItem::None;
    FindNoticeUntil = EditorialRestNoticeUntil = LobbyNoticeUntil = PhotoStatusUntil = FirstStoryThoughtUntil = 0;
    if (bFromTitle && IsValid(BackgroundMusicPlayer))
    {
        BackgroundMusicPlayer->AdjustVolume(0.30f, GameplayMusicVolume);
    }
    if (Save->Version == 1 && GetGameDay() > 30)
    {
        CreateEndingIssue(EEndingKind::Special, 30);
    }
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReporterSaveRoundTripTest, "Reporter.Save.RoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReporterSaveRoundTripTest::RunTest(const FString& Parameters)
{
    FReporterSaveState Original;
    Original.ReporterName = TEXT("Alex");
    Original.ReporterCash = 67;
    Original.OutdoorGameMinutes = 8 * 1440 + 504;
    Original.bHasDesk = true;
    Original.DeskPosition = FVector2D(0.37, 0.82);
    Original.bSponsorshipClaimed = true;
    Original.OpeningMailRead[1] = true;
    Original.ActiveFallCase = 0;
    auto& Fall = Original.FallCases.AddDefaulted_GetRef();
    Fall.Day = 9;
    Fall.bCheckedGround = true;
    Fall.LatestPhoto = 1;
    auto& Loss = Original.LostCases.AddDefaulted_GetRef();
    Loss.Day = 8;
    Loss.bTalkedOwner = true;
    Loss.ReplyDay = 10;

    const FString Photo = TEXT("Photo_fixture.png");
    Original.EditorPhotoFiles = {Photo, Photo};
    Original.EditorPhotoNames = {TEXT("Original"), TEXT("Cropped copy")};
    Original.CapturedPhotoFiles = {Photo};
    Original.EditorPhotoEdits.SetNum(2);
    Original.DraftPhotoEdits.SetNum(2);
    Original.DraftPhotoEdits[1].Crop = FVector4(0.2, 0.2, 0.7, 0.8);
    Original.ArticlePhotos = {1};
    Original.FallPhotoFiles.Add(Photo, 0);
    Original.FallPhotoCases.Add(1, 0);
    Original.FountainPhotos.Add(0);
    Original.PhotoBytes.Add(Photo, {47, 52, 99});
    auto& Article = Original.PublishedArticles.AddDefaulted_GetRef();
    Article.Page.Headline = TEXT("Park News");
    Article.Page.Photos = {1};
    Article.Page.Edits = {Original.DraftPhotoEdits[1]};
    Article.Page.Edits[0].Filter = 2;
    Article.StoryType = 2;
    Article.EventIndex = 0;
    Article.bPrintedEventFollowup = true;
    Article.Comments.Add({TEXT("Cal V."), TEXT("Thanks for checking the pavement."), 2});
    Original.bNewsroomComments = true;
    Original.DailyArticleRevenue.Add(9, 7);
    Original.EndingIssue.Kind = decltype(Original.EndingIssue.Kind)::Special;
    Original.EndingIssue.Day = 30;
    Original.EndingIssue.Stories = {2, 5, 9, 12};
    Original.EndingIssue.Captions = {TEXT("Editor"), TEXT("Repaired paving"), TEXT("Trusted"), TEXT("Rent covered")};
    Original.ExhaustedNights = 1;
    Original.ReporterHealth = 76;
    Original.BirdReports = 1;
    Original.BirdNextDay = 8;
    Original.BirdSeenRound = 2;
    Original.bBirdGlassKnown = true;
    Original.OwnedFurniture = FIntVector(1, 2, 5);
    Original.FoodStock = FIntVector(3, 0, 2);
    Original.EquippedFurniture = FIntVector(1, 2, 3);
    Original.bInLobby = Original.bInMall = true;
    Original.MallPosition = FVector2D(0.21, 0.88);

    UReporterSaveSlot* Save = NewObject<UReporterSaveSlot>();
    Save->Version = SaveVersion;
    {
        FMemoryWriter Writer(Save->GameState, true);
        Original.Serialize(Writer, Save->Version);
        if (!TestFalse(TEXT("State writer"), Writer.IsError())) { return false; }
    }
    TArray<uint8> GameFile;
    if (!TestTrue(TEXT("SaveGame wrapper writes"), UGameplayStatics::SaveGameToMemory(Save, GameFile))) { return false; }
    const UReporterSaveSlot* Loaded = Cast<UReporterSaveSlot>(UGameplayStatics::LoadGameFromMemory(GameFile));
    if (!TestNotNull(TEXT("SaveGame wrapper loads"), Loaded)) { return false; }

    FReporterSaveState Result;
    FMemoryReader Reader(Loaded->GameState, true);
    Result.Serialize(Reader, Loaded->Version);
    if (!TestTrue(TEXT("State reader consumed the payload"), Reader.AtEnd() && !Reader.IsError()) ||
        !TestTrue(TEXT("Photo indices are valid"), Result.HasValidPhotoIndices()) ||
        !TestEqual(TEXT("Fall cases"), Result.FallCases.Num(), 1) ||
        !TestEqual(TEXT("Lost cases"), Result.LostCases.Num(), 1) ||
        !TestEqual(TEXT("Published articles"), Result.PublishedArticles.Num(), 1))
    {
        return false;
    }
    TestEqual(TEXT("Cash"), Result.ReporterCash, 67);
    TestEqual(TEXT("Furniture placement"), Result.DeskPosition, Original.DeskPosition);
    TestTrue(TEXT("Read mail and claimed funds"), Result.OpeningMailRead[1] && Result.bSponsorshipClaimed);
    TestTrue(TEXT("Fall investigation"), Result.FallCases[0].bCheckedGround);
    TestTrue(TEXT("Lost property investigation"), Result.LostCases[0].bTalkedOwner);
    TestEqual(TEXT("Photo links"), Result.FallPhotoCases.FindRef(1), 0);
    TestEqual(TEXT("Archive filter"), Result.PublishedArticles[0].Page.Edits[0].Filter, 2);
    TestTrue(TEXT("Furniture ownership, placed models and mall position survive reload"),
        Result.HasValidFurniture() && Result.OwnedFurniture == Original.OwnedFurniture &&
        Result.EquippedFurniture == Original.EquippedFurniture && Result.bInMall &&
        Result.MallPosition == Original.MallPosition);
    TestEqual(TEXT("Food stacks survive reload"), Result.FoodStock, Original.FoodStock);
    TestEqual(TEXT("Health survives reload independently of exhaustion"), Result.ReporterHealth, 76);
    TestTrue(TEXT("Second bird investigation survives reload"), Result.BirdReports == 1 &&
        Result.BirdNextDay == 8 && Result.BirdSeenRound == 2 && Result.bBirdGlassKnown && Result.BirdFixDay == 0);
    const auto& SavedArticle = Result.PublishedArticles[0];
    TestTrue(TEXT("Reader comments, event association and open tab survive reload"),
        Result.bNewsroomComments && SavedArticle.StoryType == 2 && SavedArticle.EventIndex == 0 &&
        SavedArticle.bPrintedEventFollowup && SavedArticle.Comments.Num() == 1 &&
        SavedArticle.Comments[0].Reader == TEXT("Cal V.") &&
        SavedArticle.Comments[0].Body == TEXT("Thanks for checking the pavement.") && SavedArticle.Comments[0].Day == 2);
    TestTrue(TEXT("Ending archive survives reload"), Result.HasValidEnding() &&
        Result.EndingIssue.Day == Original.EndingIssue.Day &&
        Result.EndingIssue.Stories == Original.EndingIssue.Stories &&
        Result.EndingIssue.Captions == Original.EndingIssue.Captions &&
        Result.ExhaustedNights == Original.ExhaustedNights);
    TestTrue(TEXT("Embedded photo shared by copies"), Result.EditorPhotoFiles == Original.EditorPhotoFiles &&
        Result.PhotoBytes.FindRef(Photo) == Original.PhotoBytes.FindRef(Photo));

    TArray<uint8> LegacyPayload;
    FMemoryWriter LegacyWriter(LegacyPayload, true);
    Original.Serialize(LegacyWriter, 1);
    FReporterSaveState Legacy;
    FMemoryReader LegacyReader(LegacyPayload, true);
    Legacy.Serialize(LegacyReader, 1);
    TestTrue(TEXT("Version 1 layout still loads with default ending state"),
        LegacyReader.AtEnd() && !LegacyReader.IsError() && Legacy.HasValidEnding() &&
        Legacy.EndingIssue.Stories.IsEmpty() && Legacy.ExhaustedNights == 0 && Legacy.ReporterCash == Original.ReporterCash);
    TArray<uint8> V2Payload;
    FMemoryWriter V2Writer(V2Payload, true);
    Original.Serialize(V2Writer, 2);
    FReporterSaveState V2;
    FMemoryReader V2Reader(V2Payload, true);
    V2.Serialize(V2Reader, 2);
    TestTrue(TEXT("Version 2 restores the ending and safely seeds general comments"),
        V2Reader.AtEnd() && !V2Reader.IsError() && V2.HasValidEnding() && V2.EndingIssue.Day == 30 &&
        V2.PublishedArticles.Num() == 1 && V2.PublishedArticles[0].StoryType == 0 &&
        V2.PublishedArticles[0].Comments.Num() == 1 && !V2.bNewsroomComments);
    TestTrue(TEXT("Old saves retain their starter furniture"),
        V2.HasValidFurniture() && V2.EquippedFurniture == FIntVector::ZeroValue && !V2.bInMall);
    TArray<uint8> V4Payload;
    FMemoryWriter V4Writer(V4Payload, true);
    Original.Serialize(V4Writer, 4);
    FReporterSaveState V4;
    FMemoryReader V4Reader(V4Payload, true);
    V4.Serialize(V4Reader, 4);
    TestTrue(TEXT("Version 4 loads with an empty food inventory and retains furniture"),
        V4Reader.AtEnd() && !V4Reader.IsError() && V4.FoodStock == FIntVector::ZeroValue &&
        V4.OwnedFurniture == Original.OwnedFurniture && V4.ReporterHealth == 65 && !V4.bInHospital);
    return true;
}
#endif
