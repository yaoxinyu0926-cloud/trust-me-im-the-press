#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "ReporterGameMode.generated.h"

class UTexture2D;
class UPaperSprite;
class UPaperSpriteComponent;

UCLASS()
class AParkPropActor : public AActor
{
    GENERATED_BODY()

public:
    AParkPropActor();

    UFUNCTION(BlueprintCallable, Category="Park")
    static void InitializeEditorArtwork(UPaperSprite* Sprite, UTexture2D* Texture);
    virtual void OnConstruction(const FTransform& Transform) override;

    FVector2D GetBase() const;
    FVector2D GetDrawSize() const;
    bool BlocksFoot(FVector2D Position) const;

    UPROPERTY(EditAnywhere, Category="Park")
    TObjectPtr<UPaperSprite> Artwork;

    UPROPERTY(EditAnywhere, Category="Park")
    FVector2D Size = FVector2D(100, 100);

    UPROPERTY(EditAnywhere, Category="Park")
    FVector2D Footprint = FVector2D(0, 0);

private:
    UPROPERTY(VisibleAnywhere, Category="Park")
    TObjectPtr<UPaperSpriteComponent> Preview;
};

UCLASS()
class AReporterDesktopSettings : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category="Desktop", meta=(ClampMin="0.50", ClampMax="1.0"))
    float ScreenScale = 0.93f;
};

enum class EReporterFacing : uint8
{
    Front,
    Left,
    Right,
    Back
};

UCLASS()
class AReporterPawn : public APawn
{
    GENERATED_BODY()

public:
    AReporterPawn();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    FVector2D GetGroundPosition() const;
    EReporterFacing GetFacing() const { return Facing; }
    float GetWalkProgress() const { return WalkProgress; }
    float GetPreviewTime() const { return PreviewTime; }
    FVector2D GetMoveInput() const { return Input; }
    bool IsWalking() const { return bWalking; }

private:
    void MoveHorizontal(float Value) { Input.X = Value; }
    void MoveVertical(float Value) { Input.Y = Value; }

    void ToggleCamera();
    void TakePhoto();

    void ZoomPhotoIn();
    void ZoomPhotoOut();
    void SetPhotoCloseUp(bool bCloseUp);

    friend struct FReporterSaveState;
    void ToggleBackpack();
    void ToggleSettings();
    void Click();
    void RunStep(FKey Key);
    void ReleaseClick();
    void DecorShortcut(FKey Key);

    void PreviewMorning() { TargetPreviewTime = 0.0f; }
    void PreviewNoon() { TargetPreviewTime = 1.0f; }
    void PreviewEvening() { TargetPreviewTime = 2.0f; }

    float PreviewTime = 1.0f;
    float TargetPreviewTime = 1.0f;
    FVector2D Input = FVector2D::ZeroVector;
    EReporterFacing Facing = EReporterFacing::Front;
    float WalkProgress = 0.0f;
    bool bWalking = false;
};

UCLASS(Config=Game)
class AReporterHUD : public AHUD
{
    GENERATED_BODY()

public:
    AReporterHUD();
    virtual void BeginPlay() override;
    virtual void DrawHUD() override;
    virtual void Tick(float DeltaSeconds) override;
    bool IsTitleOpen() const { return TitlePage != ETitlePage::Playing; }
    bool IsReporterPanelOpen() const { return bReporterPanelOpen; }
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void ToggleBackpack();
    void ToggleSettings();
    bool IsSettingsOpen() const { return bSettingsOpen; }
    bool IsEndingOpen() const { return bEndingOpen; }
    bool IsInLobby() const { return bInLobby; }
    void Interact(bool bUsePointer);
    void RunStep(FKey Key);
    void ReleasePointer();
    void DecorShortcut(FKey Key);
    void ToggleCamera();
    void TakePhoto();
    void SetPhotoCloseUp(bool bCloseUp);

private:
    void DrawReporter(const AReporterPawn& Reporter, FVector2D Foot, float Height,
        const FVector2D* FacingOverride = nullptr, const FLinearColor& Tint = FLinearColor::White);
    friend struct FReporterSaveState;
    enum class ESettingsAction : uint8 { Open, Close, Save, Load, Confirm, Cancel, Music, Effects, Quit };
    struct FSaveSlotPreview
    {
        FString Name;
        double Minutes = 0;
        int32 Location = 0;
        bool bOccupied = false;
        bool bReadable = false;
    };
    struct FSettingsButton
    {
        FBox2D Bounds = FBox2D(ForceInit);
        ESettingsAction Action = ESettingsAction::Close;
        int32 Slot = INDEX_NONE;
    };
    bool bSettingsOpen = false;
    bool bDrawingSettings = false;
    bool bSettingsPausedWorld = false;
    ESettingsAction SettingsConfirmAction = ESettingsAction::Close;
    int32 SettingsConfirmSlot = INDEX_NONE;
    ESettingsAction SettingsSlider = ESettingsAction::Close;
    FBox2D SettingsSliderTrack = FBox2D(ForceInit);
    FBox2D SettingsShortcut = FBox2D(ForceInit);
    FSaveSlotPreview SaveSlots[3];
    TArray<FSettingsButton> SettingsButtons;
    FString SettingsFeedback;
    static constexpr float GameplayMusicVolume = 0.18f;
    float MusicVolume = 1.0f;
    float EffectsVolume = 1.0f;
    void DrawSettingsShortcut();
    void DrawSettings();
    bool ClickSettings(FVector2D Pointer);
    void CloseSettings();
    void UpdateSettingsSlider(float X);
    void LoadAudioSettings();
    void StoreAudioSettings() const;
    void ApplyAudioSettings();
    void PlayShutterSound();
    void RefreshSaveSlots();
    int32 GetSaveLocation() const;
    bool CanSaveAtCurrentLocation() const;
    bool SaveReporterGame(int32 Slot);
    bool LoadReporterGame(int32 Slot);
    TArray<FString> EditorPhotoFiles;
    TMap<FString, TArray<uint8>> RestoredPhotoBytes;
    void BeginTextFrame();
    void EndTextFrame();
    void RemoveTextLayer();
    void DrawUiText(const FString& Value, FLinearColor Color, float X, float Y, UFont* Font, float Scale = 1.0f);
    void MeasureUiText(const FString& Value, float& Width, float& Height, UFont* Font, float Scale) const;
    TSharedPtr<class FReporterTextLayer> UiTextLayer;

    void InitTitle();
    void StopBackgroundMusic();
    void DrawTitle();
    void DrawOpeningComic();
    void DrawAssembledOpeningComic();
    void AdvanceOpeningComic();
    void BeginNewsroomPlay();
    void BeginReporterCreation();
    void DrawReporterCreation();
    void RemoveReporterCreationControls();
    void RollReporterSkills();
    int32 GetFoodPrice(int32 BasePrice) const;

    enum class EReporterTalent : uint8 { Frugal, BusinessInstinct, GoodEar };
    EReporterTalent ReporterTalent = EReporterTalent::Frugal;
    FString ReporterName = TEXT("Alex Reed");
    int32 ReporterSkills[3] = {10, 10, 10};
    int32 ReporterRollsLeft = 10;
    TSharedPtr<class SConstraintCanvas> ReporterCreationControls;
    FVector2D ReporterCreationViewport = FVector2D::ZeroVector;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> ReporterCreationDesk;
    void HandleTitleClick(bool bUsePointer);

    enum class ETitlePage : uint8 { Playing, Viewfinder, Reveal, Menu, Load, Achievements, Comic, CreateReporter };
    ETitlePage TitlePage = ETitlePage::Playing;
    double TitleRevealStart = 0.0;
    int32 OpeningComicPanel = 0;
    double OpeningPanelStarted = 0.0;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> OpeningComicArt;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> OpeningComicPage;

    UPROPERTY(Config, EditDefaultsOnly, Category="Title")
    bool bAssembleOpeningComic = true;
    FBox2D TitleButtons[3];
    UPROPERTY()
    TObjectPtr<class UFont> TitleBoldFont;

    UPROPERTY()
    TObjectPtr<class UFont> TitleItalicFont;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> TitleRescue;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> TitleNewsprint;

    UPROPERTY()
    TObjectPtr<class USoundWave> BackgroundMusic;

    UPROPERTY(Transient)
    TObjectPtr<class UAudioComponent> BackgroundMusicPlayer;

    UPROPERTY(Config, EditDefaultsOnly, Category="Title")
    float TitleMastheadSize = 58.0f;

    UPROPERTY(Config, EditDefaultsOnly, Category="Title")
    float TitleSubtitleSize = 29.0f;

    UPROPERTY(Config, EditDefaultsOnly, Category="Title")
    FLinearColor TitleSubtitleColor = FLinearColor(0.18f, 0.025f, 0.023f);

    void DrawLobby(const AReporterPawn& Reporter);
    void DrawSportsLobby(const AReporterPawn& Reporter);
    void DrawHotPotInterior(const AReporterPawn& Reporter);
    void LoadHotPotChallenge();
    void DrawHotPotChallenge();
    void HandleHotPotClick(bool bUsePointer);
    void CastChopsticks();
    void UpdateHotPotChallenge(float DeltaSeconds);
    void FinishHotPotChallenge();

    enum class EHotPotState : uint8 { Idle, Order, Playing, Result };
    enum class EChopstickState : uint8 { Aiming, Extending, Retracting };
    struct FHotPotIngredient
    {
        int32 Kind = 0;
        double Radius = 0;
        double Phase = 0;
        double Speed = 0;
        double RespawnAt = 0;
        FVector2D GetPosition(double Time) const;
    };
    EHotPotState HotPotState = EHotPotState::Idle;
    EChopstickState ChopstickState = EChopstickState::Aiming;
    TArray<FHotPotIngredient> HotPotIngredients;
    FBox2D HotPotPrimary = FBox2D(ForceInit);
    FBox2D HotPotBack = FBox2D(ForceInit);
    double HotPotElapsed = 0;
    double HotPotAimTime = 0;
    double ChopstickAngle = 0;
    double ChopstickReach = 0;
    double ChopstickLimit = 0;
    int32 HeldIngredient = INDEX_NONE;
    int32 HotPotCounts[2] = {};
    int32 HotPotScore = 0;
    int32 HotPotRebate = 0;
    int32 HotPotFullnessGain = 0;
    TCHAR HotPotGrade = 'C';
    FString HotPotFeedback;
    double HotPotFeedbackUntil = 0;
    int32 ReporterCash = 0;
    int32 ReporterFullness = 56;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> HotPotTable;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> HotPotFoodArt;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> HotPotChopsticks;
    void DrawPoolDeck(const AReporterPawn& Reporter, FVector2D TopLeft, FVector2D Size);
    void DrawTreadmillRun(const AReporterPawn& Reporter, double Scale);
    void StartTreadmillRun();
    void StopTreadmillRun();
    void UpdateTreadmillRun();
    void HandleTreadmillClick(bool bUsePointer);
    bool StartGymMusic(int32 Repeats = 1);
    bool PlayActivityMusic(class USoundBase* Track);
    void StopActivityMusic();
    void StartSwimRace();
    void StopSwimRace();
    void UpdateSwimRace(float DeltaSeconds);
    void DrawSwimRace(FVector2D TopLeft, FVector2D Size);
    void HandleSwimClick(bool bUsePointer);

    enum class ESwimState : uint8 { Idle, Ready, Racing, Result };
    struct FSwimmer
    {
        double Distance = 0;
        double Speed = 0;
        double Finish = -1;
        bool bStroke = false;
    };
    ESwimState SwimState = ESwimState::Idle;
    FSwimmer Swimmers[4];
    double SwimReadyTime = 0;
    double SwimElapsed = 0;
    double SwimBreath = 1;
    bool bForcedBreath = false;
    FBox2D SwimRetry = FBox2D(ForceInit);
    FBox2D SwimExit = FBox2D(ForceInit);

    enum class ERunState : uint8 { Idle, Running, Result };
    ERunState RunState = ERunState::Idle;
    FBox2D RunRetry = FBox2D(ForceInit);
    FBox2D RunExit = FBox2D(ForceInit);
    FVector2D RunFoot = FVector2D::ZeroVector;
    double RunStarted = 0;
    double RunFeedbackUntil = 0;
    FString RunFeedback;
    int32 RunNote = 0;
    int32 RunHits = 0;
    int32 RunCombo = 0;
    int32 RunBest = 0;

    UPROPERTY()
    TObjectPtr<class UAudioComponent> ActivityMusicPlayer;
    void DrawEditorial(const AReporterPawn& Reporter);
    void DrawDesktop();
    void DrawMail(FVector2D Origin, FVector2D Size, FVector2D Close);
    bool ClickMail(FVector2D Pointer);
    bool HasUnreadMail() const;
    void DrawMailUnreadDot(FVector2D Center, double Radius);

    enum class EMailPage : uint8 { Inbox, Sent, Contacts, Compose };
    enum class EMailAction : uint8 { Inbox, Sent, Contacts, Compose, OpenLease, OpenWelcome, Claim, OpenToyOwner, OpenLostOwner };
    EMailPage MailPage = EMailPage::Inbox;
    int32 SelectedMail = INDEX_NONE;
    bool bMailRead[2] = {};
    bool bSponsorshipClaimed = false;
    TArray<TPair<FBox2D, EMailAction>> MailActions;
    void DrawStreetStatus();
    double GetHudScale() const;
    FVector2D GetHudShortcutOrigin() const;
    FBox2D DrawHudShortcut(FVector2D Pos, bool bBackpack, bool bEnabled);
    void DrawHudRoundedRect(double X, double Y, double W, double H, double Radius, FLinearColor Color);
    FBox2D DrawHudCard(double X, double Y, double W, double H, FLinearColor Fill);
    static constexpr int32 RestMinutes = 180;
    static constexpr int32 RestStamina = 60;
    static constexpr int32 DraftMinutes = 180;
    static constexpr int32 DraftStamina = 10;
    bool ClickStreetStatus(FVector2D Pointer);

    double OutdoorGameMinutes = 8 * 60 + 30;
    bool bReporterPanelOpen = false;
    bool bShowReporterSkills = false;
    FBox2D ReporterPortrait = FBox2D(ForceInit);
    FBox2D ReporterPanel = FBox2D(ForceInit);
    FBox2D ReporterTabs[2];
    void DrawStoryEditor(FVector2D Origin, FVector2D Size, FVector2D Close, FVector2D CloseSize);
    void RefreshEditorPhotos();
    bool ClickStoryEditor(FVector2D Mouse);

    struct FEditorPhoto
    {
        FVector4 Crop = FVector4(0, 0, 1, 1);
        int32 Turn = 0;
        int32 Filter = 0;
    };

    UPROPERTY()
    TArray<TObjectPtr<UTexture2D>> EditorPhotos;

    TArray<FEditorPhoto> EditorPhotoEdits;
    TArray<FEditorPhoto> DraftPhotoEdits;
    TArray<FString> EditorPhotoNames;
    FEditorPhoto WorkingPhoto;
    FBox2D CropCanvas = FBox2D(ForceInit);
    FVector2D CropStart = FVector2D::ZeroVector;
    int32 CropDrag = 0;
    FVector4 CropAtGrab = FVector4(0, 0, 1, 1);
    int32 CropRatio = 0;
    void UpdateCrop();
    void SelectEditorPhoto(int32 Index);
    void ApplyStoryTemplate();
    void ShowStoryInputs(FVector2D Origin, double Scale);
    void HideStoryInputs();
    TSharedPtr<class SWidget> StoryInputs;
    FVector2D StoryInputOrigin = FVector2D::ZeroVector;
    double StoryInputScale = 1;
    FVector2D StoryInputCanvasSize = FVector2D::ZeroVector;
    FString DraftHeadline;
    FString DraftBody;
    bool bCustomStory = false;
    int32 BodySize = 1;
    int32 BodyPage = 0;
    TArray<int32> ArticlePhotos;
    TArray<TPair<FBox2D, int32>> EditorActions;
    UPROPERTY()
    TObjectPtr<UTexture2D> FountainToy;
    FBox2D FountainToyBounds = FBox2D(ForceInit);
    FBox2D FountainQuestion = FBox2D(ForceInit);
    TSet<FString> LoadedPhotoFiles;
    TArray<FString> CapturedPhotoFiles;
    TSet<int32> FountainPhotos;
    bool bFountainDiscovered = false;
    bool bFountainReported = false;
    enum class EFoundItem : uint8 { None, Toy, Coins, Litter, Keys, Wallet, Textbook };
    enum class EItemPlace : uint8 { Away, Ground, Backpack };
    enum class EFindAction : uint8 { Bag, PickToy, PickCoins, PickLitter, KeepCoins, HandInCoins,
        BinLitter, PostNotice, PutToyBack, ReturnToy, TalkLostOwner, AskWalletFinder, PickLostItem, PostLostNotice, ReturnLostItem,
        EatBar, EatSandwich, EatMeal };
    struct FStreetFind
    {
        EItemPlace Place = EItemPlace::Away;
        FVector2D Ground = FVector2D::ZeroVector;
    };
    EItemPlace ToyPlace = EItemPlace::Ground;
    FStreetFind FoundCoins;
    FStreetFind FoundLitter;
    EFoundItem HeldItem = EFoundItem::None;
    double HeldItemUntil = 0;
    FString FindNotice;
    double FindNoticeUntil = 0;
    int32 LastFindDay = 0;
    int32 CoinsValue = 0;
    int32 ReporterMorality = 0;
    int32 ToyReplyDay = 0;
    bool bToyOwnerMailRead = false;
    bool bToyReturned = false;
    bool bPennyAchievement = false;
    bool bBackpackOpen = false;
    TArray<TPair<FBox2D, EFindAction>> FindActions;
    struct FLostCase
    {
        EFoundItem Item = EFoundItem::None;
        EItemPlace Place = EItemPlace::Ground;
        FVector2D ItemGround = FVector2D::ZeroVector;
        FVector2D SearchGround = FVector2D::ZeroVector;
        int32 Day = 0;
        int32 Owner = 0;
        int32 Finder = INDEX_NONE;
        int32 ReplyDay = 0;
        int32 LatestPhoto = INDEX_NONE;
        bool bAskedFinder = false;
        bool bTalkedOwner = false;
        bool bPhotographed = false;
        bool bMailRead = false;
        bool bReturned = false;
        bool bPublished = false;
    };
    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> LostItemArt;
    TArray<FLostCase> LostCases;
    TMap<FString, int32> WalletPhotoFiles;
    TMap<int32, int32> WalletPhotoCases;
    int32 LastLostRollDay = 0;
    int32 NextLostDay = 2;
    int32 PendingLostPhoto = INDEX_NONE;
    int32 StoryWalletCase = INDEX_NONE;
    FBox2D LostPhotoSubject = FBox2D(ForceInit);

    void UpdateLostProperty();
    void DrawLostProperty(FVector2D Center, double CameraX, double Scale, const FLinearColor& Tint);
    void DrawLostActions(FVector2D Ground,
        TFunctionRef<void(bool, EFindAction, const TCHAR*)> Action);
    bool HandleLostAction(EFindAction Action);
    bool IsLostParticipant(int32 Character) const;
    const TCHAR* LostItemName(EFoundItem Item) const;
    bool HasLostOwnerMail() const;
    FString LostOwnerMailSubject() const;
    FString LostOwnerMailSender() const;
    FString LostOwnerMailBody() const;
    void ApplyWalletTemplate();
    bool CanPublishWalletStory() const;

    void UpdateStreetFinds();
    bool HasToyOwnerMail() const;
    bool HasIntroUnreadMail() const;
    void DrawFoundItem(EFoundItem Item, FVector2D Center, double Height);
    void DrawStreetFinds(FVector2D Center, double CameraX, double Scale);
    void DrawBackpack(FVector2D Ground);
    bool ClickBackpack(FVector2D Pointer);
    bool IsNearFindProp(FVector2D Ground, FName Sprite) const;
    void SetFindNotice(const FString& Notice, EFoundItem Item = EFoundItem::None);
    bool HasPendingFountainStory() const { return !bFountainReported && !FountainPhotos.IsEmpty(); }
    enum class EFallCause : uint8 { Misstep, LooseTile, WetPaving };
    enum class EFallCrowd : uint8 { Alone, Witness, Helping };
    enum class EFallAction : uint8 { Inspect, Camera, CheckResident, CheckGround, ClosePanel };
    struct FFallCase
    {
        int32 Day = 0;
        int32 Site = 0;
        int32 Resident = 0;
        int32 Witness = INDEX_NONE;
        FVector2D Ground = FVector2D::ZeroVector;
        EFallCause Cause = EFallCause::Misstep;
        EFallCrowd Crowd = EFallCrowd::Alone;
        bool bDiscovered = false;
        bool bPanelOpen = false;
        bool bSpoke = false;
        bool bCheckedGround = false;
        bool bPublished = false;
        int32 LatestPhoto = INDEX_NONE;
    };
    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> FallSeatedPoses;
    TArray<FFallCase> FallCases;
    TMap<FString, int32> FallPhotoFiles;
    TMap<int32, int32> FallPhotoCases;
    TArray<TPair<FBox2D, EFallAction>> FallActions;
    FBox2D FallPhotoSubject = FBox2D(ForceInit);
    FVector2D FallScreenAnchor = FVector2D::ZeroVector;
    int32 ActiveFallCase = INDEX_NONE;
    int32 PendingFallPhotoCase = INDEX_NONE;
    int32 StoryFallCase = INDEX_NONE;
    int32 FallStoryPage = 0;
    int32 LastFallRollDay = 0;
    int32 NextFallDay = 3;
    int32 FallRepairDays[2] = {0, 0};

    bool IsRainyDay() const;
    void UpdateFallEvent();
    void LeaveFallSceneForBuilding();
    void DrawFallScene(FVector2D Center, double CameraX, double Scale, const FLinearColor& Tint);
    void DrawFallPrompts(FVector2D Ground);
    bool ClickFallPrompts(FVector2D Pointer);
    bool IsFallParticipant(int32 Character) const;
    bool HasVerifiedFallCause(const FFallCase& Incident) const;
    FString FallStoryLabel(int32 Index) const;
    void ApplyFallTemplate();
    bool CanPublishFallStory() const;
    void PublishFallStory();

    int32 StoryKind = INDEX_NONE;
    int32 EditorTab = 0;
    int32 PhotoIndex = 0;
    int32 PhotoPage = 0;
    int32 StoryAngle = 0;
    int32 StoryTemplate = 0;
    int32 ArticleLayout = 0;
    int32 HeadlineSize = 1;
    int32 Paper = 1;
    int32 Stamp = 0;
    bool bHeadlineBold = true;
    bool bUnderline = false;
    bool bFrontPageSaved = false;
    bool bDraftCostPaid = false;
    struct FNewspaper
    {
        FString Headline;
        FString Body;
        TArray<int32> Photos;
        TArray<FEditorPhoto> Edits;
        int32 Layout = 0;
        int32 TitleSize = 1;
        int32 BodySize = 1;
        int32 Paper = 1;
        int32 Stamp = 0;
        bool bBold = true;
        bool bUnderline = false;
    };
    struct FPublishedArticle
    {
        struct FComment
        {
            FString Reader;
            FString Body;
            int32 Day = 0;
        };
        FNewspaper Page;
        int32 Day = 1;
        int32 Views = 0;
        int32 Earnings = 0;
        int32 InitialViews = 0;
        bool bExposed = false;
        int32 Angle = 0;
        int32 StoryType = 0;
        int32 EventIndex = INDEX_NONE;
        bool bPrintedEventFollowup = false;
        TArray<FComment> Comments;
    };
    static void SeedReaderComments(FPublishedArticle& Article, int32 ArticleIndex);
    void UpdateReaderComments(FPublishedArticle& Article, int32 Day);
    FNewspaper GetDraftNewspaper() const;
    int32 DrawNewspaper(const FNewspaper& Page, FVector2D Origin, FVector2D Size,
        double Scale, bool bPrint, int32 PageIndex, int32 PublicationDay = 0);
    int32 DrawStoryParagraphs(FString Value, FVector2D Position, double Width,
        float Scale, int32 MaxLines, int32 Page = 0, int32* VisibleLines = nullptr);
    FBox2D DrawStoryPhoto(int32 Index, const FEditorPhoto& Edit, FVector2D Origin,
        FVector2D Size, bool bTopAligned);
    void PublishNewspaper();
    void AdvanceGameMinutes(double Minutes);
    int32 GetGameDay() const;
    static constexpr int32 RentAmount = 100;
    static constexpr int32 RentInterval = 7;
    static constexpr int32 ExhaustedStamina = 10;
    enum class EEndingKind : uint8 { None, Special, Hospital, Bankruptcy };
    struct FEndingIssue
    {
        EEndingKind Kind = EEndingKind::None;
        int32 Day = 0;
        TArray<int32> Stories;
        TArray<FString> Captions;
    };
    int32 GetReporterReputation() const;
    bool SettleEndingDay(int32 ClosedDay);
    void CreateEndingIssue(EEndingKind Kind, int32 Day);
    void OpenEnding(bool bArchive = false);
    void DrawEnding();
    void ClickEnding(bool bUsePointer);
    void PreviewEndingFromCommandLine();
    void DrawEndingWarnings();
    UPROPERTY()
    TObjectPtr<class USoundWave> SpecialEndingMusic;
    UPROPERTY()
    TObjectPtr<class USoundWave> FailedEndingMusic;
    FEndingIssue EndingIssue;
    int32 ExhaustedNights = 0;
    static constexpr int32 MaxHealth = 100;
    static constexpr int32 ExhaustionHealthLoss = 35;
    static constexpr int32 TreatmentPrice = 50;
    static constexpr int32 TreatmentHealth = 30;
    int32 ReporterHealth = MaxHealth;
    bool bInHospital = false;
    UPROPERTY()
    TObjectPtr<UTexture2D> HospitalInterior;
    void DrawHospital(const AReporterPawn& Reporter);
    bool TreatHealth();
    bool IsBirdSceneActive() const;
    void DrawBirdScene(FVector2D Center, double CameraX, double Scale, FLinearColor Tint);
    void UpdateBirdScene(FVector2D Ground);
    int32 GetBirdPhotoIndex() const;
    bool CanPublishBirdStory() const;
    void ApplyBirdTemplate();
    void PublishBirdStory();
    int32 BirdReports = 0;
    int32 BirdNextDay = 5;
    int32 BirdFixDay = 0;
    int32 BirdSeenRound = 0;
    bool bBirdGlassKnown = false;
    FBox2D BirdPhotoBounds = FBox2D(ForceInit);
    UPROPERTY()
    TObjectPtr<UTexture2D> InjuredBirdArt;
    UPROPERTY()
    TObjectPtr<UTexture2D> BirdSafeMallArt;
    bool bEndingOpen = false;
    bool bReadingArchivedEnding = false;
    int32 EndingPage = 0;
    FString LoadedEndingArt;
    FBox2D EndingPrevious = FBox2D(ForceInit);
    FBox2D EndingNext = FBox2D(ForceInit);
    FBox2D EndingClose = FBox2D(ForceInit);
    UPROPERTY()
    TObjectPtr<UTexture2D> EndingTexture;
#if WITH_DEV_AUTOMATION_TESTS
    friend class FReporterEndingTest;
#endif
    void DrawNewsroom(FVector2D Origin, double Scale, FVector2D Close);
    bool ClickNewsroom(FVector2D Mouse);
    TArray<FPublishedArticle> PublishedArticles;
    TMap<int32, int32> DailyArticleRevenue;
    int32 NewsroomFollowers = 0;
    int32 SelectedPublishedArticle = INDEX_NONE;
    int32 NewsroomListPage = 0;
    int32 NewsroomBodyPage = 0;
    bool bNewsroomOverview = true;
    bool bNewsroomComments = false;
    TArray<TPair<FBox2D, int32>> NewsroomActions;
    bool ClickDesktop(FVector2D Mouse);

    UPROPERTY()
    TObjectPtr<AReporterDesktopSettings> DesktopSettings;

    UPROPERTY()
    TObjectPtr<UTexture2D> DesktopWallpaper;

    UPROPERTY()
    TArray<TObjectPtr<UTexture2D>> DesktopIcons;

    FBox2D DesktopButtons[3];
    FBox2D DesktopClose = FBox2D(ForceInit);
    bool bDesktopOpen = false;
    int32 DesktopPage = INDEX_NONE;
    void DrawDecorControls();
    void BeginNewsroomSetup();
    void DrawNewsroomSetupGuide(FVector2D TopLeft, FVector2D Size);
    void DrawBedRestAction(FVector2D TopLeft, FVector2D Size);
    bool IsNearEditorialBed() const;
    void RestAtEditorialBed(bool bUntilMorning = false);
    void DrawMorningRestConfirmation();
    void ClickMorningRestConfirmation(bool bUsePointer);
    bool bConfirmMorningRest = false;
    FBox2D MorningRestButton = FBox2D(ForceInit);
    FBox2D MorningRestConfirm = FBox2D(ForceInit);
    FBox2D MorningRestCancel = FBox2D(ForceInit);

    bool bNewsroomSetupGuide = false;
    bool bFirstStoryGuide = false;
    bool bShowFirstStoryResults = false;
    double FirstStoryThoughtUntil = 0.0;
    FString LatestCapturedFountainPhoto;
    FString LatestImportedFountainPhoto;
    int32 LatestFountainPhotoIndex = INDEX_NONE;
    void DrawFirstStoryGuide();
    FBox2D SkipSetupGuideButton = FBox2D(ForceInit);
    int32 ReporterStamina = 72;
    FString EditorialRestNotice;
    double EditorialRestNoticeUntil = 0;
    bool ClickDecor(FVector2D Mouse);
    void BeginDecorating();
    void AddDesk();
    void RotateDesk();
    void FinishDecorating();
    void UpdateDeskDrag();
    void ConstrainBed();
    FVector2D GetBedRadius() const;
    void ConstrainComputer();
    FVector2D GetComputerFoot() const;
    FVector2D GetComputerTravel() const;
    void DrawComputer();
    FBox2D GetDeskBounds(FVector2D TopLeft, FVector2D Size) const;
    void ConstrainDesk();
    enum class EFurniture : uint8 { Bed, Desk, Computer };
    struct FFurnitureView
    {
        UTexture2D* Texture = nullptr;
        FVector2D UV = FVector2D::ZeroVector;
        FVector2D UVSize = FVector2D::UnitVector;
        double Aspect = 1.0;
    };
    FFurnitureView GetFurnitureView(EFurniture Kind, int32 Tier, int32 Turn);
    void DrawFurnitureView(const FFurnitureView& Art, FBox2D Bounds, bool bMirror = false, double Angle = 0);
    void DrawMall(const AReporterPawn& Reporter);
    void DrawFurnitureShop();
    void ClickFurnitureShop(bool bUsePointer);
    bool BuyFurniture(EFurniture Kind, int32 Tier);
    bool OwnsFurniture(EFurniture Kind, int32 Tier) const;
    void CycleFurniture(EFurniture Kind);
    int32 FurnitureCost(EFurniture Kind, int32 Base) const;
    FString FurnitureLabel(EFurniture Kind) const;
    static FString FurnitureDuration(int32 Minutes);
    FIntVector OwnedFurniture = FIntVector::ZeroValue;
    FIntVector EquippedFurniture = FIntVector::ZeroValue;
    FVector2D MallPosition = FVector2D(0.50, 0.92);
    bool bInMall = false;
    bool bFurnitureShopOpen = false;
    bool bFoodShopOpen = false;
    FIntVector FoodStock = FIntVector::ZeroValue;
    int32 SelectedFood = 0;
    int32 FoodQuantity = 1;
    UPROPERTY()
    TObjectPtr<UTexture2D> FoodAtlas;
    FBox2D FoodCards[3];
    FBox2D FoodMinus, FoodPlus, FoodBuy, FoodClose;
    static constexpr int32 MaxFoodStock = 99;
    bool IsMallShopOpen() const { return bFurnitureShopOpen || bFoodShopOpen; }
    void OpenFoodShop();
    void DrawFoodShop();
    void ClickFoodShop(bool bUsePointer);
    void DrawFoodIcon(int32 Index, FVector2D Center, FVector2D MaxSize);
    void DrawBackpackFood(FVector2D Position, double Scale);
    bool BuyFood(int32 Index, int32 Quantity);
    bool EatFood(int32 Index);
#if WITH_DEV_AUTOMATION_TESTS
    friend class FReporterFoodTest;
#endif
    EFurniture FurnitureShopCategory = EFurniture::Bed;
    int32 FurnitureShopTier = 1;
    UPROPERTY()
    TObjectPtr<UTexture2D> MallInterior;
    UPROPERTY()
    TMap<FString, TObjectPtr<UTexture2D>> FurnitureTextures;
    FBox2D ShopTabs[3];
    FBox2D ShopCards[3];
    FBox2D ShopBuy = FBox2D(ForceInit);
    FBox2D ShopClose = FBox2D(ForceInit);
    FBox2D ComputerFurnitureButton = FBox2D(ForceInit);
#if WITH_DEV_AUTOMATION_TESTS
    friend class FReporterFurnitureTest;
#endif

    UPROPERTY()
    TArray<TObjectPtr<UTexture2D>> DeskViews;

    UPROPERTY()
    TObjectPtr<UTexture2D> ComputerFront;

    UPROPERTY()
    TObjectPtr<UTexture2D> ComputerSide;

    UPROPERTY()
    TArray<TObjectPtr<UTexture2D>> BedViews;

    FBox2D BedButton = FBox2D(ForceInit);
    FBox2D BedBounds = FBox2D(ForceInit);
    FVector2D BedPosition = FVector2D(0.22, 0.87);
    double BedScale = 1.0;
    int32 BedTurn = 0;
    bool bHasBed = false;
    bool bBedSelected = false;
    bool bDraggingBed = false;
    bool bRotatingBed = false;
    double BedAngle = 0.0;
    double BedAngleAtGrab = 0.0;
    double PointerAngleAtGrab = 0.0;
    FBox2D BedRotationHandle = FBox2D(ForceInit);
    FBox2D ComputerBounds = FBox2D(ForceInit);
    FVector2D ComputerPosition = FVector2D(0.0, -0.25);
    FVector2D ComputerDragOffset = FVector2D::ZeroVector;
    double DeskScale = 1.0;
    double ComputerScale = 1.20;
    bool bDraggingComputer = false;

    FBox2D DecorButton = FBox2D(ForceInit);
    FBox2D DeskButton = FBox2D(ForceInit);
    FBox2D RotateButton = FBox2D(ForceInit);
    FBox2D DoneButton = FBox2D(ForceInit);
    FBox2D DeskBounds = FBox2D(ForceInit);
    FVector2D DeskPosition = FVector2D(0.45, 0.83);
    FVector2D DecorRoomOrigin = FVector2D::ZeroVector;
    FVector2D DecorRoomSize = FVector2D::ZeroVector;
    FVector2D DragOffset = FVector2D::ZeroVector;
    int32 DeskTurn = 0;
    bool bHasDesk = false;
    bool bDecorating = false;
    bool bDraggingDesk = false;
    FString DecorStatus;
    void DrawInteraction(const TCHAR* Label, FVector2D Marker);
    FIntRect GetPhotoFrame() const;

    enum class EInteraction : uint8 { None, Enter, EnterSportsCenter, EnterHotPot, OrderHotPot, EnterGym, ExitGym, EnterPool, ExitPool, StartSwim, StartRun, Exit, Reception, Editorial, ExitEditorial, RestAtBed, Stairs, Locked, EnterMall, BrowseFurniture, BrowseFood, EnterHospital, TreatHealth };
    EInteraction Interaction = EInteraction::None;
    FBox2D InteractionButton = FBox2D(ForceInit);
    FBox2D InteractionMarker = FBox2D(ForceInit);
    const AParkPropActor* FindNearbyEntrance(FVector2D Ground, EInteraction Entrance, double* DoorX = nullptr) const;

    bool bInLobby = false;
    bool bInHotPot = false;
    bool bInSportsCenter = false;
    bool bInGym = false;
    bool bInPool = false;
    FVector2D PoolPosition;
    bool bInEditorial = false;
    bool bUpstairs = false;
    bool bShowGuide = false;
    bool bIndoorWalking = false;
    double LobbyX = 0.10;
    FVector2D EditorialPosition = FVector2D(0.89, 0.79);
    double StairProgress = -1.0;
    FVector2D IndoorDirection = FVector2D(1, 0);
    FString LobbyNotice;
    double LobbyNoticeUntil = 0.0;

    UPROPERTY()
    TObjectPtr<UTexture2D> LobbyTexture;

    UPROPERTY()
    TObjectPtr<UTexture2D> SportsLobbyTexture;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> HotPotTexture;

    UPROPERTY()
    TObjectPtr<UTexture2D> GymTexture;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> PoolTexture;

    UPROPERTY()
    TArray<TObjectPtr<AParkPropActor>> GymProps;

    UPROPERTY()
    TObjectPtr<UTexture2D> EditorialTexture;
    void UpdatePhotoPan();
    void DrawCameraOverlay();

    bool bCameraMode = false;
    bool bPhotoCloseUp = false;
    FVector2D PhotoFrameCenter = FVector2D(0.5, 0.5);
    FString PendingPhoto;
    FString PhotoStatus;
    double PhotoStatusUntil = 0.0;

    UPROPERTY()
    TObjectPtr<UTexture2D> PoseAtlas;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> FemalePoseAtlas;

    struct FStreetPedestrian
    {
        int32 Character = INDEX_NONE;
        FVector2D Ground = FVector2D::ZeroVector;
        FString Speech;
        double WalkTime = 0;
        double SecondsUntilPause = 0;
        double PauseSeconds = 0;
        double SpeechSeconds = 0;
        bool bWalkRight = false;
        bool bGreetingChecked = false;
    };

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> PedestrianAtlases;
    TArray<FStreetPedestrian> StreetPedestrians;
    void UpdateStreetPedestrians(float DeltaSeconds, FVector2D PlayerGround);
    void DrawPedestrianSpeech(const FString& Speech, FVector2D Head);
    int32 PedestrianDay = 0;
    bool bRecheckPedestriansOnStreet = true;

    bool bFemaleReporter = false;
    UTexture2D* GetActivePoseAtlas() const;

    UPROPERTY()
    TObjectPtr<UTexture2D> PaintedGround;

    UPROPERTY()
    TObjectPtr<UTexture2D> DistantTown;

    UPROPERTY()
    TArray<TObjectPtr<AParkPropActor>> ParkProps;
};

UCLASS()
class AReporterGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AReporterGameMode();
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
