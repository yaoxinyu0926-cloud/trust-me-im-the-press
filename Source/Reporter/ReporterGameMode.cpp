#include "ReporterGameMode.h"

#include "Misc/ScopeExit.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "CanvasItem.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerStart.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "TextureResource.h"

namespace
{
constexpr float ReferenceHeight = 720.0f;
constexpr float CharacterHeight = 180.0f;
struct FPedestrianCharacter
{
    const TCHAR* AtlasFile;
    double HeightScale;
    double Speed;
    double FrameSeconds;
    const TCHAR* Lines[4];
};
const FPedestrianCharacter PedestrianCharacters[] = {
    {TEXT("YoungPedestrianPoses.png"), 1.04, 53, 0.20,
        {TEXT("Well, I messed that up again."), TEXT("I could use a coffee."),
         TEXT("Wonder what's new in town."), TEXT("I've got time for the long way.")}},
    {TEXT("YoungWomanPedestrianPoses.png"), 1.00, 52, 0.20,
        {TEXT("Did I leave my keys at home?"), TEXT("I needed some fresh air."),
         TEXT("That shop window looks different."), TEXT("Maybe I'll take the long way.")}},
    {TEXT("OlderWomanPedestrianPoses.png"), 0.94, 39, 0.26,
        {TEXT("The roses will be out soon."), TEXT("That bench gets the best sun."),
         TEXT("I used to walk this way to school."), TEXT("The park is quiet today.")}},
    {TEXT("OlderManPedestrianPoses.png"), 1.00, 43, 0.24,
        {TEXT("I remember when that shop opened."), TEXT("I miss the old paper stand."),
         TEXT("My watch is slow again."), TEXT("I'll do one more lap.")}},
    {TEXT("ExpectingWomanPedestrianPoses.png"), 1.00, 41, 0.25,
        {TEXT("We still haven't picked a name."), TEXT("I think I'll sit for a bit."),
         TEXT("That bakery smells wonderful."), TEXT("A short walk sounds nice.")}},
    {TEXT("BoyPedestrianPoses.png"), 0.75, 53, 0.18,
        {TEXT("I bet I can count all the pigeons."), TEXT("I can skip the cracks!"),
         TEXT("I wish school got out earlier."), TEXT("I found a really cool rock.")}},
    {TEXT("GirlPedestrianPoses.png"), 0.73, 50, 0.18,
        {TEXT("That cloud looks like a bunny."), TEXT("I made a drawing of the park."),
         TEXT("I hope the ducks are out."), TEXT("I know a shortcut!")}}
};
constexpr float WalkSpeed = 230.0f;
constexpr float WalkCycleDistance = 208.0f;
// Furniture is laid out in a separate editor area using the room image's pixel coordinates.
const FName GymEquipmentTag(TEXT("GymEquipment"));
// Gym props use background pixel coordinates in a separate area of the editor.
const FVector2D GymLayoutOrigin(5500, 0);
const FName EditorialFurnitureTag(TEXT("EditorialFurniture"));
constexpr double EditorialWidth = 1774.0;
constexpr double EditorialHeight = 887.0;
constexpr double DeskWidth = 370.0 * 0.75;
constexpr double DeskHeight = 170.0;
constexpr double DeskSideWidth = 140.0;

FVector2D RotateTablePoint(FVector2D Point, int32 Turn)
{
    switch (Turn)
    {
    case 1: return FVector2D(-Point.Y, Point.X);
    case 2: return -Point;
    case 3: return FVector2D(Point.Y, -Point.X);
    default: return Point;
    }
}

double EditorialBackEdge(double X)
{
    return 0.674 + FMath::Max(FMath::Max(0.0, 0.197 - X) * 0.73,
        FMath::Max(0.0, X - 0.804) * 0.66);
}
constexpr double StreetModuleWidth = 864.0;
constexpr int32 StreetFirstModule = -4;
constexpr int32 StreetEndModule = 3;
constexpr double StreetLeft = StreetFirstModule * StreetModuleWidth;
constexpr double StreetRight = StreetEndModule * StreetModuleWidth;
constexpr double PavementBack = 72.0;
constexpr double CurbTop = PavementBack + (360.0 - PavementBack) * 535.0 / 724.0;
constexpr double FootMargin = 8.0;
constexpr double DepthSpeedFactor = 0.55;

enum EStreetColor
{
    SkyTop, SkyHorizon, FarHills, NearHills, River, Cloud, Paving,
    Mortar, Curb, Asphalt, RoadPaint, StreetColorCount
};

const TCHAR* const StreetColors[4][StreetColorCount] = {
    {TEXT("779ACB"), TEXT("F9CFA7"), TEXT("9494B2"), TEXT("757F9B"), TEXT("91AAB7"), TEXT("FFF1D9"),
        TEXT("E6B89B"), TEXT("A57D73"), TEXT("BD8D83"), TEXT("766C86"), TEXT("D5B7AA")},
    {TEXT("5C9DE7"), TEXT("C5E8F4"), TEXT("8FADC5"), TEXT("719BB4"), TEXT("89B9CD"), TEXT("FFF5E3"),
        TEXT("E9BA97"), TEXT("A37D70"), TEXT("BC8C85"), TEXT("766780"), TEXT("F3DFC0")},
    {TEXT("525483"), TEXT("F4AE88"), TEXT("A07B9D"), TEXT("756988"), TEXT("A285A3"), TEXT("F9C7C0"),
        TEXT("B98E85"), TEXT("795C6B"), TEXT("8C697C"), TEXT("524B6D"), TEXT("CBB3A4")},
    {TEXT("131D3D"), TEXT("34466A"), TEXT("283852"), TEXT("233047"), TEXT("344561"), TEXT("435575"),
        TEXT("4A4B66"), TEXT("354052"), TEXT("3F4059"), TEXT("242B45"), TEXT("8189A5")}
};

struct FReporterPose
{
    FIntRect Region;
    float CenterX;
    float WaistY;
};

// Atlas columns are front, left, back. The first row is standing.
const FReporterPose Poses[3][3] = {
    {
        {FIntRect(156, 11, 310, 402), 243, 249},
        {FIntRect(518, 11, 673, 402), 592, 249},
        {FIntRect(913, 11, 1063, 401), 986, 250}
    },
    {
        {FIntRect(157, 434, 317, 822), 243, 665},
        {FIntRect(485, 433, 702, 821), 592, 665},
        {FIntRect(910, 433, 1066, 822), 986, 668}
    },
    {
        {FIntRect(155, 854, 315, 1243), 243, 1086},
        {FIntRect(479, 853, 703, 1239), 592, 1087},
        {FIntRect(911, 853, 1070, 1243), 986, 1089}
    }
};

const FReporterPose FemalePoses[3][3] = {
    {
        {FIntRect(148, 13, 314, 402), 239, 250},
        {FIntRect(513, 12, 683, 401), 590, 249},
        {FIntRect(906, 12, 1069, 401), 988, 250}
    },
    {
        {FIntRect(142, 435, 320, 821), 239, 665},
        {FIntRect(480, 433, 704, 818), 591, 665},
        {FIntRect(906, 434, 1070, 822), 988, 666}
    },
    {
        {FIntRect(149, 855, 314, 1243), 239, 1086},
        {FIntRect(476, 853, 707, 1241), 591, 1087},
        {FIntRect(907, 855, 1069, 1244), 988, 1086}
    }
};

struct FSpriteVertex
{
    FVector2D Position;
    FVector2D UV;
};

void AddSpriteTriangle(TArray<FCanvasUVTri>& Triangles, const FSpriteVertex& A,
    const FSpriteVertex& B, const FSpriteVertex& C, const FLinearColor& Tint)
{
    FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
    Triangle.V0_Pos = A.Position;
    Triangle.V1_Pos = B.Position;
    Triangle.V2_Pos = C.Position;
    Triangle.V0_UV = A.UV;
    Triangle.V1_UV = B.UV;
    Triangle.V2_UV = C.UV;
    Triangle.V0_Color = Tint;
    Triangle.V1_Color = Tint;
    Triangle.V2_Color = Tint;
}

void AddColorQuad(TArray<FCanvasUVTri>& Triangles, FVector2D A, FVector2D B,
    FVector2D C, FVector2D D, FLinearColor Top, FLinearColor Bottom)
{
    const FVector2D UV(0.5, 0.5);
    AddSpriteTriangle(Triangles, {A, UV}, {B, UV}, {C, UV}, Top);
    Triangles.Last().V2_Color = Bottom;
    AddSpriteTriangle(Triangles, {B, UV}, {D, UV}, {C, UV}, Bottom);
    Triangles.Last().V0_Color = Top;
}

void AddCloudPuff(TArray<FCanvasUVTri>& Triangles, FVector2D Center,
    FVector2D Radius, FLinearColor Tint)
{
    constexpr int32 Segments = 24;
    const FVector2D UV(0.5, 0.5);
    for (int32 Segment = 0; Segment < Segments; ++Segment)
    {
        const double A = UE_TWO_PI * Segment / Segments;
        const double B = UE_TWO_PI * (Segment + 1) / Segments;
        AddSpriteTriangle(Triangles, {Center, UV},
            {Center + Radius * FVector2D(FMath::Cos(A), FMath::Sin(A)), UV},
            {Center + Radius * FVector2D(FMath::Cos(B), FMath::Sin(B)), UV}, Tint);
    }
}

void DrawSky(UCanvas& Canvas, double Scale, double Time, const FLinearColor* Colors, bool bDrawHills)
{
    const double Width = Canvas.ClipX;
    const double Horizon = Canvas.ClipY * 0.5 + PavementBack * Scale;
    TArray<FCanvasUVTri> Triangles;
    Triangles.Reserve(350);
    AddColorQuad(Triangles, {0, 0}, {Width, 0}, {0, Horizon}, {Width, Horizon},
        Colors[SkyTop], Colors[SkyHorizon]);

    for (int32 CloudIndex = 0; CloudIndex < 3; ++CloudIndex)
    {
        const double CloudScale = Scale * (0.70 + 0.16 * CloudIndex);
        const double Loop = Width + 300.0 * Scale;
        const double X = FMath::Fmod(Width * (0.20 + 0.36 * CloudIndex)
            + Time * (7.0 + 2.5 * CloudIndex) * Scale, Loop) - 120.0 * Scale;
        const double Y = (82.0 + 30.0 * (CloudIndex % 2)) * Scale;
        AddCloudPuff(Triangles, {X, Y}, {91 * CloudScale, 13 * CloudScale}, Colors[Cloud]);
        AddCloudPuff(Triangles, {X - 30 * CloudScale, Y - 7 * CloudScale},
            {34 * CloudScale, 20 * CloudScale}, Colors[Cloud]);
        AddCloudPuff(Triangles, {X + 7 * CloudScale, Y - 15 * CloudScale},
            {38 * CloudScale, 26 * CloudScale}, Colors[Cloud]);
        AddCloudPuff(Triangles, {X + 43 * CloudScale, Y - 6 * CloudScale},
            {29 * CloudScale, 16 * CloudScale}, Colors[Cloud]);
    }

    if (!bDrawHills)
    {
        FCanvasTriangleItem Item(Triangles, Canvas.DefaultTexture->GetResource());
        Canvas.DrawItem(Item);
        return;
    }

    for (int32 Layer = 0; Layer < 2; ++Layer)
    {
        auto Ridge = [Horizon, Scale, Layer](double Fraction)
        {
            const double Wave = FMath::Sin(Fraction * 9.0 + Layer)
                + 0.38 * FMath::Sin(Fraction * 21.0 - Layer);
            return Horizon - (75.0 - 30.0 * Layer + 21.0 * Wave) * Scale;
        };
        constexpr int32 Segments = 36;
        const FLinearColor Tint = Colors[Layer == 0 ? FarHills : NearHills];
        for (int32 Segment = 0; Segment < Segments; ++Segment)
        {
            const double A = double(Segment) / Segments;
            const double B = double(Segment + 1) / Segments;
            AddColorQuad(Triangles, {Width * A, Ridge(A)}, {Width * B, Ridge(B)},
                {Width * A, Horizon}, {Width * B, Horizon}, Tint, Tint);
        }
    }
    AddColorQuad(Triangles, {0, Horizon - 19 * Scale}, {Width, Horizon - 19 * Scale},
        {0, Horizon}, {Width, Horizon}, Colors[River], Colors[River]);

    FCanvasTriangleItem Item(Triangles, Canvas.DefaultTexture->GetResource());
    Canvas.DrawItem(Item);
}

template <typename Warp>
void DrawSpriteMesh(UCanvas& Canvas, UTexture2D& Texture, const FIntRect& Region,
    const FLinearColor& Tint, Warp&& PositionAt)
{
    constexpr int32 Columns = 8;
    constexpr int32 Rows = 20;
    FSpriteVertex Vertices[Rows + 1][Columns + 1];
    const FVector2D TextureSize(Texture.GetSizeX(), Texture.GetSizeY());
    for (int32 Row = 0; Row <= Rows; ++Row)
    {
        for (int32 Column = 0; Column <= Columns; ++Column)
        {
            const FVector2D Pixel(
                FMath::Lerp<float>(Region.Min.X, Region.Max.X, float(Column) / Columns),
                FMath::Lerp<float>(Region.Min.Y, Region.Max.Y, float(Row) / Rows));
            Vertices[Row][Column] = {PositionAt(Pixel), Pixel / TextureSize};
        }
    }

    TArray<FCanvasUVTri> Triangles;
    Triangles.Reserve(Rows * Columns * 2);
    for (int32 Row = 0; Row < Rows; ++Row)
    {
        for (int32 Column = 0; Column < Columns; ++Column)
        {
            const FSpriteVertex& A = Vertices[Row][Column];
            const FSpriteVertex& B = Vertices[Row][Column + 1];
            const FSpriteVertex& C = Vertices[Row + 1][Column];
            const FSpriteVertex& D = Vertices[Row + 1][Column + 1];
            AddSpriteTriangle(Triangles, A, B, C, Tint);
            AddSpriteTriangle(Triangles, B, D, C, Tint);
        }
    }

    FCanvasTriangleItem Item(Triangles, Texture.GetResource());
    Item.BlendMode = SE_BLEND_Translucent;
    Canvas.DrawItem(Item);
}
}

AReporterPawn::AReporterPawn()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    PrimaryActorTick.bCanEverTick = true;
}

void AReporterPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis(TEXT("MoveHorizontal"), this, &AReporterPawn::MoveHorizontal);
    PlayerInputComponent->BindAxis(TEXT("MoveVertical"), this, &AReporterPawn::MoveVertical);
    PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AReporterPawn::PreviewMorning);
    PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AReporterPawn::PreviewNoon);
    PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AReporterPawn::PreviewEvening);
    PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &AReporterPawn::ToggleCamera);
    PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &AReporterPawn::ToggleBackpack);
    PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AReporterPawn::ToggleSettings).bExecuteWhenPaused = true;
    PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AReporterPawn::Click).bExecuteWhenPaused = true;
    PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AReporterPawn::ReleaseClick).bExecuteWhenPaused = true;
    for (const FKey& Key : {EKeys::E, EKeys::F, EKeys::R})
    {
        PlayerInputComponent->BindKey(Key, IE_Pressed, this, &AReporterPawn::DecorShortcut);
    }
    for (const FKey& Key : {EKeys::A, EKeys::D, EKeys::Q, EKeys::SpaceBar})
    {
        PlayerInputComponent->BindKey(Key, IE_Pressed, this, &AReporterPawn::RunStep).bConsumeInput = false;
    }
    PlayerInputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AReporterPawn::TakePhoto);
    PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AReporterPawn::ZoomPhotoIn);
    PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AReporterPawn::ZoomPhotoOut);
    PlayerInputComponent->BindKey(EKeys::Equals, IE_Pressed, this, &AReporterPawn::ZoomPhotoIn);
    PlayerInputComponent->BindKey(EKeys::Hyphen, IE_Pressed, this, &AReporterPawn::ZoomPhotoOut);
}

FVector2D AReporterPawn::GetGroundPosition() const
{
    return FVector2D(GetActorLocation());
}

void AReporterPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    PreviewTime = FMath::FInterpConstantTo(PreviewTime, TargetPreviewTime, DeltaSeconds, 2.2f);

    if (APlayerController* Player = Cast<APlayerController>(GetController()))
    {
        if (const AReporterHUD* HUD = Player->GetHUD<AReporterHUD>(); HUD && (HUD->IsInLobby() || HUD->IsReporterPanelOpen() || HUD->IsTitleOpen() || HUD->IsSettingsOpen() || HUD->IsEndingOpen()))
        {
            return;
        }
    }

    if (!Input.IsNearlyZero())
    {
        if (Input.X != 0.0)
        {
            Facing = Input.X < 0.0 ? EReporterFacing::Left : EReporterFacing::Right;
        }
        else
        {
            Facing = Input.Y < 0.0 ? EReporterFacing::Back : EReporterFacing::Front;
        }
    }

    const FVector2D Previous = GetGroundPosition();
    const FVector2D Direction = Input.GetClampedToMaxSize(1.0);
    FVector2D Position = Previous + FVector2D(Direction.X, Direction.Y * DepthSpeedFactor)
        * WalkSpeed * DeltaSeconds;
    Position.X = FMath::Clamp(Position.X, StreetLeft + 58.0, StreetRight - 58.0);
    Position.Y = FMath::Clamp(Position.Y, PavementBack + FootMargin, CurbTop - FootMargin);
    const auto IsParkPropBlocked = [this](FVector2D Point)
    {
        for (TActorIterator<AParkPropActor> Prop(GetWorld()); Prop; ++Prop)
        {
            if (!Prop->ActorHasTag(EditorialFurnitureTag) && !Prop->ActorHasTag(GymEquipmentTag) && Prop->BlocksFoot(Point))
            {
                return true;
            }
        }
        return false;
    };
    // Resolve each axis independently so the reporter can slide around prop bases.
    FVector2D Resolved = Previous;
    if (!IsParkPropBlocked({Position.X, Previous.Y}))
    {
        Resolved.X = Position.X;
    }
    if (!IsParkPropBlocked({Resolved.X, Position.Y}))
    {
        Resolved.Y = Position.Y;
    }
    Position = Resolved;
    SetActorLocation(FVector(Position, 0.0));
    const double Distance = FVector2D::Distance(Position, Previous);
    bWalking = Distance > UE_SMALL_NUMBER;
    WalkProgress = bWalking
        ? FMath::Fmod(WalkProgress + static_cast<float>(Distance) / WalkCycleDistance, 1.0f)
        : 0.0f;
}

void AParkPropActor::InitializeEditorArtwork(UPaperSprite* Sprite, UTexture2D* Texture)
{
#if WITH_EDITOR
    if (Sprite && Texture)
    {
        FSpriteAssetInitParameters Parameters;
        Parameters.SetTextureAndFill(Texture);
        Sprite->InitializeSprite(Parameters);
        Sprite->MarkPackageDirty();
    }
#endif
}

void AReporterPawn::ToggleCamera()
{
    if (APlayerController* Player = Cast<APlayerController>(GetController()))
    {
        if (AReporterHUD* HUD = Player->GetHUD<AReporterHUD>())
        {
            HUD->ToggleCamera();
        }
    }
}

void AReporterPawn::ToggleSettings()
{
    if (APlayerController* Player = Cast<APlayerController>(GetController()))
    {
        if (AReporterHUD* HUD = Player->GetHUD<AReporterHUD>())
        {
            HUD->ToggleSettings();
        }
    }
}

void AReporterPawn::ToggleBackpack()
{
    if (APlayerController* Player = Cast<APlayerController>(GetController()))
    {
        if (AReporterHUD* HUD = Player->GetHUD<AReporterHUD>())
        {
            HUD->ToggleBackpack();
        }
    }
}

void AReporterPawn::Click()
{
    if (APlayerController* Player = Cast<APlayerController>(GetController()))
    {
        if (AReporterHUD* HUD = Player->GetHUD<AReporterHUD>())
        {
            HUD->Interact(true);
        }
    }
}

void AReporterPawn::ReleaseClick()
{
    if (APlayerController* Player = Cast<APlayerController>(GetController()))
    {
        if (AReporterHUD* HUD = Player->GetHUD<AReporterHUD>())
        {
            HUD->ReleasePointer();
        }
    }
}

void AReporterPawn::DecorShortcut(FKey Key)
{
    if (APlayerController* Player = Cast<APlayerController>(GetController()))
    {
        if (AReporterHUD* HUD = Player->GetHUD<AReporterHUD>())
        {
            HUD->DecorShortcut(Key);
        }
    }
}

void AReporterPawn::RunStep(FKey Key)
{
    if (APlayerController* Player = Cast<APlayerController>(GetController()))
    {
        if (AReporterHUD* HUD = Player->GetHUD<AReporterHUD>())
        {
            HUD->RunStep(Key);
        }
    }
}

void AReporterPawn::TakePhoto()
{
    if (APlayerController* Player = Cast<APlayerController>(GetController()))
    {
        if (AReporterHUD* HUD = Player->GetHUD<AReporterHUD>())
        {
            HUD->Interact(false);
        }
    }
}

AParkPropActor::AParkPropActor()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Preview = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Editor Preview"));
    Preview->SetupAttachment(RootComponent);
    Preview->SetHiddenInGame(true);
    Preview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AParkPropActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Preview->SetSprite(Artwork);
    if (!Artwork)
    {
        return;
    }

    const FBoxSphereBounds Bounds = Artwork->GetRenderBounds();
    const double Width = FMath::Max(Bounds.BoxExtent.X * 2.0, 1.0);
    const double Height = FMath::Max(Bounds.BoxExtent.Z * 2.0, 1.0);
    const FVector2D PreviewScale(Size.X / Width, Size.Y / Height);
    Preview->SetRelativeRotation(FRotator(0, 0, -90));
    Preview->SetRelativeScale3D(FVector(PreviewScale.X, 1, PreviewScale.Y));
    Preview->SetRelativeLocation(FVector(-Bounds.Origin.X * PreviewScale.X,
        -(Bounds.Origin.Z - Bounds.BoxExtent.Z) * PreviewScale.Y, 0));
}

FVector2D AParkPropActor::GetBase() const
{
    return FVector2D(GetActorLocation());
}

FVector2D AParkPropActor::GetDrawSize() const
{
    const FVector Scale = GetActorScale3D();
    return Size * FVector2D(FMath::Abs(Scale.X), FMath::Abs(Scale.Y));
}

bool AParkPropActor::BlocksFoot(FVector2D Position) const
{
    if (Footprint.X <= 0 || Footprint.Y <= 0)
    {
        return false;
    }
    const FVector Scale = GetActorScale3D();
    const FVector2D Radius = Footprint * FVector2D(FMath::Abs(Scale.X), FMath::Abs(Scale.Y));
    const FVector2D Delta = Position - (GetBase() - FVector2D(0, Radius.Y));
    return (Delta / (Radius + FVector2D(8, 7))).SizeSquared() < 1.0;
}

void AReporterHUD::BeginPlay()
{
    Super::BeginPlay();
    LoadAudioSettings();
    InitTitle();
    LoadHotPotChallenge();
    for (const TCHAR* File : {TEXT("DeskFront.png"), TEXT("DeskSide.png"), TEXT("DeskBack.png")})
    {
        DeskViews.Add(FImageUtils::ImportFileAsTexture2D(
            FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt") / File));
    }
    const FString ArtPath = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt");
    MallInterior = FImageUtils::ImportFileAsTexture2D(ArtPath / TEXT("RivermereMallInterior.png"));
    HospitalInterior = FImageUtils::ImportFileAsTexture2D(ArtPath / TEXT("HospitalInterior.png"));
    InjuredBirdArt = FImageUtils::ImportFileAsTexture2D(ArtPath / TEXT("InjuredBird.png"));
    BirdSafeMallArt = FImageUtils::ImportFileAsTexture2D(ArtPath / TEXT("RivermereMallBirdSafe.png"));
    FoodAtlas = FImageUtils::ImportFileAsTexture2D(ArtPath / TEXT("ConvenienceFood.png"));
    DesktopWallpaper = FImageUtils::ImportFileAsTexture2D(ArtPath / TEXT("DesktopWallpaper.jpg"));
    for (const TCHAR* Icon : {TEXT("DesktopMail.png"), TEXT("DesktopEditor.png"), TEXT("DesktopNewsroom.png")})
    {
        DesktopIcons.Add(FImageUtils::ImportFileAsTexture2D(ArtPath / Icon));
    }
    for (TActorIterator<AReporterDesktopSettings> Settings(GetWorld()); Settings; ++Settings)
    {
        DesktopSettings = *Settings;
        break;
    }

    FountainToy = FImageUtils::ImportFileAsTexture2D(ArtPath / TEXT("FountainToy.png"));

    ComputerFront = FImageUtils::ImportFileAsTexture2D(ArtPath / TEXT("ComputerFront.png"));
    ComputerSide = FImageUtils::ImportFileAsTexture2D(ArtPath / TEXT("ComputerSide.png"));

    for (const TCHAR* File : {TEXT("BedFront.png"), TEXT("BedSide.png"), TEXT("BedBack.png")})
    {
        BedViews.Add(FImageUtils::ImportFileAsTexture2D(ArtPath / File));
    }
    FString SavedDesk;
    if (FFileHelper::LoadFileToString(SavedDesk, *(FPaths::ProjectSavedDir() / TEXT("EditorialDesk.txt"))))
    {
        TArray<FString> Fields;
        SavedDesk.ParseIntoArray(Fields, TEXT(","), false);
        if ((Fields.Num() == 3 || Fields.Num() == 7 || Fields.Num() == 13 || Fields.Num() == 14) && Fields[0].IsNumeric() && Fields[1].IsNumeric() && Fields[2].IsNumeric())
        {
            DeskPosition = FVector2D(FCString::Atod(*Fields[0]), FCString::Atod(*Fields[1]));
            DeskTurn = FMath::Clamp(FCString::Atoi(*Fields[2]), 0, 3);
            if (Fields.Num() >= 7)
            {
                DeskScale = FMath::Clamp(FCString::Atod(*Fields[3]), 0.65, 1.35);
                ComputerScale = FMath::Clamp(FCString::Atod(*Fields[4]), 0.60, 1.40);
                ComputerPosition = FVector2D(FCString::Atod(*Fields[5]), FCString::Atod(*Fields[6]));
            }
            ConstrainDesk();
            ConstrainComputer();
            bHasDesk = Fields.Num() < 13 || Fields[7] == TEXT("1");
            if (Fields.Num() >= 13 && Fields[8] == TEXT("1"))
            {
                bHasBed = true;
                BedPosition = FVector2D(FCString::Atod(*Fields[9]), FCString::Atod(*Fields[10]));
                BedTurn = FMath::Clamp(FCString::Atoi(*Fields[11]), 0, 3);
                BedScale = FMath::Clamp(FCString::Atod(*Fields[12]), 0.65, 1.35);
                BedAngle = Fields.Num() == 14 && Fields[13].IsNumeric()
                    ? FMath::Fmod(FCString::Atod(*Fields[13]), 360.0) : 0.0;
                ConstrainBed();
            }
        }
    }

    const FString LobbyPath = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt/NewsCenterLobby.png");
    LobbyTexture = FImageUtils::ImportFileAsTexture2D(LobbyPath);
    const FString SportsPath = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt/SportsCenterLobby.png");
    SportsLobbyTexture = FImageUtils::ImportFileAsTexture2D(SportsPath);
    const FString HotPotPath = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt/RiversideHotPotInterior.jpg");
    HotPotTexture = FImageUtils::ImportFileAsTexture2D(HotPotPath);
    if (!HotPotTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not load Hot Pot interior: %s"), *HotPotPath);
    }
    const FString GymPath = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt/SportsCenterGym.png");
    GymTexture = FImageUtils::ImportFileAsTexture2D(GymPath);
    const FString PoolPath = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt/SportsCenterPool.jpg");
    PoolTexture = FImageUtils::ImportFileAsTexture2D(PoolPath);
    if (!PoolTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not load Pool: %s"), *PoolPath);
    }
    if (!GymTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not load Gym: %s"), *GymPath);
    }
    if (!SportsLobbyTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not load Sports Center lobby: %s"), *SportsPath);
    }
    const FString EditorialPath = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt/EditorialRoom.png");
    EditorialTexture = FImageUtils::ImportFileAsTexture2D(EditorialPath);
    if (!EditorialTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not load Editorial room: %s"), *EditorialPath);
    }
    if (!LobbyTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not load News Center lobby: %s"), *LobbyPath);
    }
    if (APlayerController* Player = GetOwningPlayerController())
    {
        Player->bShowMouseCursor = true;
    }

    const FString GroundPath = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt/PaintedGround.png");
    PaintedGround = FImageUtils::ImportFileAsTexture2D(GroundPath);
    if (!PaintedGround)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not load painted ground: %s"), *GroundPath);
    }
    const FString TownPath = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt/DistantTown.png");
    DistantTown = FImageUtils::ImportFileAsTexture2D(TownPath);
    if (
DistantTown
)
    {

DistantTown
->Filter = TF_Nearest;

DistantTown
->UpdateResource();
    }
    if (!DistantTown)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not load distant town: %s"), *TownPath);
    }

    for (TActorIterator<AParkPropActor> Prop(GetWorld()); Prop; ++Prop)
    {
        if (Prop->ActorHasTag(GymEquipmentTag))
        {
            GymProps.Add(*Prop);
        }
        else
        {
            ParkProps.Add(*Prop);
        }
    }
    ParkProps.Sort([](const AParkPropActor& A, const AParkPropActor& B)
    {
        return A.GetBase().Y < B.GetBase().Y;
    });
    const FString PoseArtPath = FPaths::ProjectContentDir() / TEXT("Reporter/RuntimeArt");
    const auto LoadPoseAtlas = [&](const TCHAR* File)
    {
        const FString Path = PoseArtPath / File;
        UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(Path);
        if (Texture)
        {
            Texture->Filter = TF_Nearest;
            Texture->UpdateResource();
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Could not load reporter poses: %s"), *Path);
        }
        return Texture;
    };
    PoseAtlas = LoadPoseAtlas(TEXT("ReporterPoses.png"));
    FemalePoseAtlas = LoadPoseAtlas(TEXT("ReporterFemalePoses.png"));
    for (const FPedestrianCharacter& Character : PedestrianCharacters)
    {
        PedestrianAtlases.Add(LoadPoseAtlas(Character.AtlasFile));
    }
    for (int32 Index = 0; Index < PedestrianAtlases.Num(); ++Index)
    {
        FallSeatedPoses.Add(LoadPoseAtlas(*FString::Printf(TEXT("PedestrianFall%d.png"), Index)));
    }
    for (const TCHAR* File : {TEXT("LostKeys.png"), TEXT("LostWallet.png"), TEXT("LostTextbook.png")})
    {
        LostItemArt.Add(LoadPoseAtlas(File));
    }
    PreviewEndingFromCommandLine();
#if !UE_BUILD_SHIPPING
    const bool bPreviewConvenience = FParse::Param(FCommandLine::Get(), TEXT("ReporterConveniencePreview"));
    if (MallInterior && (bPreviewConvenience || FParse::Param(FCommandLine::Get(), TEXT("ReporterFurniturePreview"))))
    {
        TitlePage = ETitlePage::Playing;
        bInLobby = bInMall = true;
        MallPosition = FVector2D(bPreviewConvenience ? 0.79 : 0.27, 0.87);
        ReporterCash = 120;
    }
#endif
}

void AReporterPawn::ZoomPhotoIn()
{
    SetPhotoCloseUp(true);
}

void AReporterPawn::ZoomPhotoOut()
{
    SetPhotoCloseUp(false);
}

void AReporterPawn::SetPhotoCloseUp(bool bCloseUp)
{
    if (APlayerController* Player = Cast<APlayerController>(GetController()))
    {
        if (AReporterHUD* HUD = Player->GetHUD<AReporterHUD>())
        {
            HUD->SetPhotoCloseUp(bCloseUp);
        }
    }
}

void AReporterHUD::SetPhotoCloseUp(bool bCloseUp)
{
    if (bCameraMode && PendingPhoto.IsEmpty())
    {
        bPhotoCloseUp = bCloseUp;
        const FIntRect Frame = GetPhotoFrame();
        const FIntPoint Size = GetWorld()->GetGameViewport()->Viewport->GetSizeXY();
        if (Size.X > 0 && Size.Y > 0)
        {
            PhotoFrameCenter = FVector2D(
                (Frame.Min.X + Frame.Width() * 0.5) / Size.X,
                (Frame.Min.Y + Frame.Height() * 0.5) / Size.Y);
        }
    }
}

void AReporterHUD::UpdatePhotoPan()
{
    if (!bCameraMode || !PendingPhoto.IsEmpty())
    {
        return;
    }
    const APlayerController* Player = GetOwningPlayerController();
    if (!Player)
    {
        return;
    }

    const FVector2D Direction(
        int32(Player->IsInputKeyDown(EKeys::Right)) - int32(Player->IsInputKeyDown(EKeys::Left)),
        int32(Player->IsInputKeyDown(EKeys::Down)) - int32(Player->IsInputKeyDown(EKeys::Up)));
    if (Direction.IsZero())
    {
        return;
    }

    const FIntRect Frame = GetPhotoFrame();
    const FIntPoint Size = GetWorld()->GetGameViewport()->Viewport->GetSizeXY();
    if (Size.X > 0 && Size.Y > 0)
    {
        const FVector2D Margin(Frame.Width() * 0.5 / Size.X, Frame.Height() * 0.5 / Size.Y);
        PhotoFrameCenter += Direction.GetSafeNormal() * 0.42 * GetWorld()->GetDeltaSeconds();
        PhotoFrameCenter.X = FMath::Clamp(PhotoFrameCenter.X, Margin.X, 1.0 - Margin.X);
        PhotoFrameCenter.Y = FMath::Clamp(PhotoFrameCenter.Y, Margin.Y, 1.0 - Margin.Y);
    }
}

void AReporterHUD::ToggleCamera()
{
    if (!IsTitleOpen() && !bEndingOpen && !bInLobby && !bReporterPanelOpen)
    {
        bBackpackOpen = false;
        bCameraMode = !bCameraMode;
        Interaction = EInteraction::None;
    }
}

FIntRect AReporterHUD::GetPhotoFrame() const
{
    const UGameViewportClient* Client = GetWorld()->GetGameViewport();
    const FIntPoint Size = Client && Client->Viewport ? Client->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
    const double Coverage = bPhotoCloseUp ? 0.45 : 0.80;
    const int32 Units = FMath::FloorToInt(FMath::Min(Size.X / 16.0, Size.Y / 9.0) * Coverage);
    const int32 Width = Units * 16;
    const int32 Height = Units * 9;
    const FIntPoint TopLeft(
        FMath::Clamp(FMath::RoundToInt(PhotoFrameCenter.X * Size.X - Width * 0.5), 0, Size.X - Width),
        FMath::Clamp(FMath::RoundToInt(PhotoFrameCenter.Y * Size.Y - Height * 0.5), 0, Size.Y - Height));
    return FIntRect(TopLeft, TopLeft + FIntPoint(Width, Height));
}

void AReporterHUD::TakePhoto()
{
    if (!bCameraMode || !PendingPhoto.IsEmpty() || FScreenshotRequest::IsScreenshotRequested())
    {
        return;
    }
    const FIntRect Frame = GetPhotoFrame();
    if (Frame.IsEmpty())
    {
        return;
    }

    const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Photos"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FDateTime Now = FDateTime::Now();
    const FBox2D Viewfinder(FVector2D(Frame.Min), FVector2D(Frame.Max));
    const bool bToyInFrame = bFountainDiscovered && !bFountainReported && FountainToyBounds.bIsValid
        && Viewfinder.IsInsideOrOn(FountainToyBounds.GetCenter());
    const bool bBirdInFrame = IsBirdSceneActive() && BirdPhotoBounds.bIsValid &&
        Viewfinder.IsInsideOrOn(BirdPhotoBounds.GetCenter());
    const FString Subject = bBirdInFrame ? FString::Printf(TEXT("_Bird%d"), BirdReports + 1)
        : FString(bToyInFrame ? TEXT("_FountainToy") : TEXT(""));
    const FString Name = FString::Printf(TEXT("Photo_%s_%03d%s.png"),
        *Now.ToString(TEXT("%Y%m%d_%H%M%S")), Now.GetMillisecond(), *Subject);
    PendingFallPhotoCase = FallCases.IsValidIndex(ActiveFallCase) && FallCases[ActiveFallCase].bDiscovered
        && FallPhotoSubject.bIsValid && Viewfinder.IsInsideOrOn(FallPhotoSubject.GetCenter())
        ? ActiveFallCase : INDEX_NONE;
    PendingLostPhoto = !LostCases.IsEmpty() && LostPhotoSubject.bIsValid &&
        Viewfinder.IsInsideOrOn(LostPhotoSubject.GetCenter()) ? LostCases.Num() - 1 : INDEX_NONE;
    PendingPhoto = Directory / Name;
    FScreenshotRequest::RequestScreenshot(PendingPhoto, false, false, false, Frame, true);
    PlayShutterSound();
}

void AReporterHUD::DrawCameraOverlay()
{
    if (!PendingPhoto.IsEmpty())
    {
        if (FScreenshotRequest::IsScreenshotRequested())
        {
            return;
        }
        const bool bSaved = IFileManager::Get().FileSize(*PendingPhoto) > 0;
        PhotoStatus = bSaved ? TEXT("Photo saved") : TEXT("Could not save photo");
        UE_LOG(LogTemp, Display, TEXT("%s: %s"), *PhotoStatus, *PendingPhoto);
        PhotoStatusUntil = GetWorld()->GetRealTimeSeconds() + 2.5;
        if (bSaved)
        {
            const FString File = FPaths::GetCleanFilename(PendingPhoto);
            CapturedPhotoFiles.AddUnique(File);
            if (File.EndsWith(TEXT("_Bird1.png")) || File.EndsWith(TEXT("_Bird2.png")))
            {
                BirdSeenRound = BirdReports + 1;
                bBirdGlassKnown |= BirdReports == 1 || ReporterSkills[1] >= 41;
                PhotoStatus = bBirdGlassKnown ? TEXT("Bird strike evidence saved.") : TEXT("Injured bird photo saved.");
            }
            if (LostCases.IsValidIndex(PendingLostPhoto))
            {
                FLostCase& Loss = LostCases[PendingLostPhoto];
                Loss.bPhotographed = true;
                if (Loss.Finder != INDEX_NONE && Loss.Day == GetGameDay() && Loss.Place == EItemPlace::Ground)
                {
                    WalletPhotoFiles.Add(File, PendingLostPhoto);
                }
            }
            if (FallCases.IsValidIndex(PendingFallPhotoCase))
            {
                FallPhotoFiles.Add(File, PendingFallPhotoCase);
                PhotoStatus = TEXT("Fall photo saved. Retakes are open until you go indoors.");
            }
        }
        if (bSaved && PendingPhoto.EndsWith(TEXT("_FountainToy.png")))
        {
            if (bFirstStoryGuide && LatestCapturedFountainPhoto.IsEmpty())
            {
                FirstStoryThoughtUntil = GetWorld()->GetRealTimeSeconds() + 7.0;
                bCameraMode = false;
            }
            LatestCapturedFountainPhoto = FPaths::GetCleanFilename(PendingPhoto);
        }
        PendingPhoto.Reset();
        PendingFallPhotoCase = INDEX_NONE;
        PendingLostPhoto = INDEX_NONE;
    }

    if (bCameraMode)
    {
        const FIntRect Frame = GetPhotoFrame();
        const FLinearColor Shade(0, 0, 0, 0.48f);
        DrawRect(Shade, 0, 0, Canvas->ClipX, Frame.Min.Y);
        DrawRect(Shade, 0, Frame.Max.Y, Canvas->ClipX, Canvas->ClipY - Frame.Max.Y);
        DrawRect(Shade, 0, Frame.Min.Y, Frame.Min.X, Frame.Height());
        DrawRect(Shade, Frame.Max.X, Frame.Min.Y, Canvas->ClipX - Frame.Max.X, Frame.Height());
        DrawLine(Frame.Min.X, Frame.Min.Y, Frame.Max.X, Frame.Min.Y, FLinearColor::White, 2.0f);
        DrawLine(Frame.Max.X, Frame.Min.Y, Frame.Max.X, Frame.Max.Y, FLinearColor::White, 2.0f);
        DrawLine(Frame.Max.X, Frame.Max.Y, Frame.Min.X, Frame.Max.Y, FLinearColor::White, 2.0f);
        DrawLine(Frame.Min.X, Frame.Max.Y, Frame.Min.X, Frame.Min.Y, FLinearColor::White, 2.0f);
        const FString Hint = FString::Printf(
            TEXT("%s  |  Arrows: Aim  Wheel / +/-: Zoom  Click / Enter: Shoot  C: Exit"),
            bPhotoCloseUp ? TEXT("Close-up") : TEXT("Wide"));
        DrawUiText(Hint,
            FLinearColor::White, Frame.Min.X, Frame.Max.Y + 12.0f, GEngine->GetSmallFont());
    }
    if (GetWorld()->GetRealTimeSeconds() < PhotoStatusUntil)
    {
        DrawUiText(PhotoStatus, FLinearColor::White, 20.0f, 20.0f, GEngine->GetSmallFont());
    }
}

void AReporterHUD::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bSettingsOpen || bEndingOpen || bConfirmMorningRest) { return; }
    UpdateTreadmillRun();
    UpdateSwimRace(DeltaSeconds);
    UpdateHotPotChallenge(DeltaSeconds);
    if (const AReporterPawn* Reporter = Cast<AReporterPawn>(GetOwningPawn()); !IsTitleOpen() && !bInLobby && Reporter)
    {
        constexpr double MinutesPerDay = 1440.0;
        constexpr double SecondsPerDay = 120.0;
        if (PendingPhoto.IsEmpty())
        {
            AdvanceGameMinutes(DeltaSeconds * MinutesPerDay / SecondsPerDay);
            if (bEndingOpen) { return; }
        }
        UpdateFallEvent();
        UpdateStreetPedestrians(DeltaSeconds, Reporter->GetGroundPosition());
        UpdateStreetFinds();
        UpdateLostProperty();
        UpdateBirdScene(Reporter->GetGroundPosition());
    }
}

void AReporterHUD::UpdateStreetPedestrians(float DeltaSeconds, FVector2D PlayerGround)
{
    constexpr double LeftEnd = StreetLeft + FootMargin;
    constexpr double RightEnd = StreetRight - FootMargin;
    const int32 Day = GetGameDay();
    const bool bNewDay = PedestrianDay != Day;
    PedestrianDay = Day;
    if (bNewDay || bRecheckPedestriansOnStreet)
    {
        const bool bArriving = bRecheckPedestriansOnStreet;
        bRecheckPedestriansOnStreet = false;
        TArray<FStreetPedestrian> Previous = MoveTemp(StreetPedestrians);
        if (!bArriving)
        {
            StreetPedestrians = Previous;
        }
        TArray<int32, TInlineAllocator<UE_ARRAY_COUNT(PedestrianCharacters)>> Candidates;
        for (int32 Index = 0; Index < PedestrianAtlases.Num(); ++Index)
        {
            if (PedestrianAtlases[Index] && !StreetPedestrians.ContainsByPredicate(
                [Index](const FStreetPedestrian& Existing) { return Existing.Character == Index; }))
            {
                Candidates.Add(Index);
            }
        }

        // Two independent coin flips give a 25/50/25 distribution for 0, 1 or 2 walkers.
        const int32 Target = int32(FMath::RandBool()) + int32(FMath::RandBool());
        while (StreetPedestrians.Num() < Target && !Candidates.IsEmpty())
        {
            const int32 Pick = FMath::RandRange(0, Candidates.Num() - 1);
            const int32 Character = Candidates[Pick];
            Candidates.RemoveAtSwap(Pick);
            const FStreetPedestrian* Existing = Previous.FindByPredicate([Character](const FStreetPedestrian& Walker)
            {
                return Walker.Character == Character;
            });
            if (Existing)
            {
                FStreetPedestrian& Walker = StreetPedestrians.Add_GetRef(*Existing);
                Walker.SpeechSeconds = 0;
                continue;
            }

            const bool bWalkRight = StreetPedestrians.IsEmpty() ? FMath::RandBool() : !StreetPedestrians[0].bWalkRight;
            FStreetPedestrian& Walker = StreetPedestrians.AddDefaulted_GetRef();
            Walker.Character = Character;
            Walker.bWalkRight = bWalkRight;
            const double StartX = bArriving ? PlayerGround.X + (bWalkRight ? -350.0 : 350.0)
                : (bWalkRight ? LeftEnd : RightEnd);
            const double Depth = bWalkRight ? 29.0 : 58.0;
            Walker.Ground = FVector2D(FMath::Clamp(StartX, LeftEnd, RightEnd), CurbTop - Depth);
            Walker.SecondsUntilPause = FMath::FRandRange(4.0f, 8.0f);
        }
        StreetPedestrians.Sort([](const FStreetPedestrian& A, const FStreetPedestrian& B)
        {
            return A.Ground.Y < B.Ground.Y;
        });
    }

    bool bSpeechActive = StreetPedestrians.ContainsByPredicate([](const FStreetPedestrian& Walker)
    {
        return Walker.SpeechSeconds > 0;
    });
    for (FStreetPedestrian& Walker : StreetPedestrians)
    {
        const FPedestrianCharacter& Character = PedestrianCharacters[Walker.Character];
        const double TimePaused = FMath::Min(Walker.PauseSeconds, double(DeltaSeconds));
        Walker.PauseSeconds -= TimePaused;
        const double WalkStep = FMath::Min(Walker.SecondsUntilPause, DeltaSeconds - TimePaused);
        Walker.Ground.X += (Walker.bWalkRight ? Character.Speed : -Character.Speed) * WalkStep;
        Walker.WalkTime += WalkStep;
        Walker.SecondsUntilPause -= WalkStep;
        if (Walker.SecondsUntilPause <= 0)
        {
            Walker.PauseSeconds = FMath::FRandRange(1.0f, 2.8f);
            Walker.SecondsUntilPause = FMath::FRandRange(4.0f, 8.0f);
            Walker.WalkTime = 0;
        }

        Walker.SpeechSeconds = FMath::Max(0.0, Walker.SpeechSeconds - DeltaSeconds);
        const FVector2D Distance = Walker.Ground - PlayerGround;
        if (!Walker.bGreetingChecked && !bSpeechActive && !bCameraMode && !bReporterPanelOpen &&
            Walker.Ground.X >= LeftEnd && Walker.Ground.X <= RightEnd &&
            FMath::Abs(Distance.X) < 112 && FMath::Abs(Distance.Y) < 65)
        {
            Walker.bGreetingChecked = true;
            if (FMath::FRand() < 0.65f)
            {
                const double Hour = FMath::Fmod(OutdoorGameMinutes, 1440.0) / 60.0;
                const int32 Line = FMath::RandRange(0, UE_ARRAY_COUNT(Character.Lines));
                Walker.Speech = Line == UE_ARRAY_COUNT(Character.Lines)
                    ? (Hour >= 6 && Hour < 12 ? TEXT("Morning!") : Hour >= 18 ? TEXT("Evening.") : TEXT("Hey there."))
                    : Character.Lines[Line];
                Walker.SpeechSeconds = 2.7;
                bSpeechActive = true;
            }
        }
    }
    StreetPedestrians.RemoveAll([&](const FStreetPedestrian& Walker)
    {
        return Walker.Ground.X < LeftEnd || Walker.Ground.X > RightEnd;
    });
}

void AReporterHUD::DrawPedestrianSpeech(const FString& Speech, FVector2D Head)
{
    const double S = GetHudScale();
    const FVector2D Padding(11 * S, 6 * S);
    float Width, Height;
    UFont* Font = GEngine->GetMediumFont();
    MeasureUiText(Speech, Width, Height, Font, S);
    const FVector2D Size = FVector2D(Width, Height) + Padding * 2;
    const double X = FMath::Clamp(Head.X - Size.X * 0.5, 8 * S, Canvas->ClipX - Size.X - 8 * S);
    const FVector2D Min(X, Head.Y - Size.Y - 12 * S);
    const FLinearColor Ink(0.17f, 0.11f, 0.08f);
    const FLinearColor BubbleFill(0.98f, 0.91f, 0.75f);
    DrawRect(Ink, Min.X, Min.Y, Size.X, Size.Y);
    DrawRect(BubbleFill, Min.X + S, Min.Y + S, Size.X - 2 * S, Size.Y - 2 * S);
    DrawUiText(Speech, Ink, Min.X + Padding.X, Min.Y + Padding.Y, Font, S);
}

void AReporterHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas)
    {
        return;
    }

    FindActions.Reset();
    FallActions.Reset();
    SettingsShortcut = FBox2D(ForceInit);
    MorningRestButton = FBox2D(ForceInit);
    BeginTextFrame();
    ON_SCOPE_EXIT
    {
        if (!bEndingOpen && (bSettingsOpen || TitlePage == ETitlePage::Load))
        {
            TGuardValue<bool> DrawingSettings(bDrawingSettings, true);
            DrawSettings();
        }
        else if (!bEndingOpen && !bConfirmMorningRest && !IsMallShopOpen() && !IsTitleOpen()) { DrawSettingsShortcut(); }
        EndTextFrame();
    };

    if (bEndingOpen)
    {
        DrawEnding();
        return;
    }
    if (bConfirmMorningRest)
    {
        DrawMorningRestConfirmation();
        return;
    }

    if (IsTitleOpen())
    {
        DrawTitle();
        return;
    }

    UpdatePhotoPan();
    const AReporterPawn* Reporter = Cast<AReporterPawn>(GetOwningPawn());
    Interaction = EInteraction::None;
    if (Reporter && bInLobby)
    {
        if (bInHospital)
        {
            DrawHospital(*Reporter);
        }
        else if (bInMall)
        {
            DrawMall(*Reporter);
        }
        else if (bInHotPot)
        {
            DrawHotPotInterior(*Reporter);
        }
        else if (bInSportsCenter)
        {
            DrawSportsLobby(*Reporter);
        }
        else if (bInEditorial)
        {
            DrawEditorial(*Reporter);
        }
        else
        {
            DrawLobby(*Reporter);
        }
        if (!bDesktopOpen && !IsMallShopOpen() && RunState == ERunState::Idle && SwimState == ESwimState::Idle &&
            HotPotState == EHotPotState::Idle)
        {
            DrawStreetStatus();
            DrawFirstStoryGuide();
            DrawBackpack(Reporter->GetGroundPosition());
        }
        return;
    }
    const FVector2D Ground = Reporter ? Reporter->GetGroundPosition() : FVector2D::ZeroVector;
    const double Scale = Canvas->ClipY / ReferenceHeight;
    const FVector2D Center(Canvas->ClipX * 0.5, Canvas->ClipY * 0.5);
    const double StreetCenter = (StreetLeft + StreetRight) * 0.5;
    const double CameraTravel = FMath::Max(0.0, (StreetRight - StreetLeft) * 0.5 - Center.X / Scale);
    const double CameraX = FMath::Clamp(Ground.X, StreetCenter - CameraTravel, StreetCenter + CameraTravel);

    const double Hour = FMath::Fmod(OutdoorGameMinutes, 1440.0) / 60.0;
    // Dawn begins at 05:30. The night palette holds from 21:00 through midnight.
    const auto BlendHours = [Hour](double Start, double End)
    {
        return FMath::SmoothStep(Start, End, Hour);
    };
    const double Light = Hour < 7.0 ? 3.0 + BlendHours(5.5, 7.0)
        : Hour < 17.0 ? BlendHours(7.0, 9.0)
        : Hour < 19.5 ? 1.0 + BlendHours(17.0, 19.5)
        : 2.0 + BlendHours(19.5, 21.0);
    const int32 Palette = FMath::FloorToInt(Light) % 4;
    const int32 NextPalette = (Palette + 1) % 4;
    const double Blend = FMath::Frac(Light);
    FLinearColor Colors[StreetColorCount];
    for (int32 Index = 0; Index < StreetColorCount; ++Index)
    {
        Colors[Index] = FMath::Lerp(FLinearColor(FColor::FromHex(StreetColors[Palette][Index])),
            FLinearColor(FColor::FromHex(StreetColors[NextPalette][Index])), Blend);
    }
    DrawRect(Colors[FarHills], 0, 0, Canvas->ClipX, Canvas->ClipY);
    DrawSky(*Canvas, Scale, GetWorld()->GetTimeSeconds(), Colors, !DistantTown);

    const FLinearColor StreetTints[] = {
        FLinearColor(1.0f, 0.93f, 0.81f), FLinearColor::White,
        FLinearColor(0.85f, 0.65f, 0.67f), FLinearColor(0.19f, 0.25f, 0.40f)
    };
    const FLinearColor GroundTint = FMath::Lerp(StreetTints[Palette], StreetTints[NextPalette], Blend);
    if (DistantTown)
    {
        constexpr double TownHeight = 380.0;
        constexpr double TownParallax = 0.22;
        const double Height = TownHeight * Scale;
        const double Width = Height * DistantTown->GetSizeX() / DistantTown->GetSizeY();
        const double Origin = Center.X + (StreetModuleWidth * 0.25 - CameraX * TownParallax) * Scale;
        const double Y = Center.Y + PavementBack * Scale - Height;
        const int32 FirstTile = FMath::FloorToInt(-Origin / Width);
        const int32 LastTile = FMath::CeilToInt((Canvas->ClipX - Origin) / Width);
        for (int32 Tile = FirstTile; Tile < LastTile; ++Tile)
        {
            const bool bMirror = Tile % 2 != 0;
            DrawTexture(DistantTown, Origin + Tile * Width, Y, Width, Height,
                bMirror ? 1.0f : 0.0f, 0, bMirror ? -1.0f : 1.0f, 1,
                GroundTint, BLEND_Translucent);
        }
    }

    for (int32 Module = StreetFirstModule; Module < StreetEndModule; ++Module)
    {
        const double X = Center.X + (Module * StreetModuleWidth - CameraX) * Scale;
        const double Y = Center.Y + PavementBack * Scale;
        const double Width = StreetModuleWidth * Scale;
        const double Height = (ReferenceHeight * 0.5 - PavementBack) * Scale;
        if (PaintedGround)
        {
            // Anchor alternating mirroring at world X = 0 so extensions keep existing seams.
            const bool bMirrorGround = Module % 2 == 0;
            DrawTexture(PaintedGround, X, Y, Width, Height,
                bMirrorGround ? 1.0f : 0.0f, 0, bMirrorGround ? -1.0f : 1.0f, 1,
                GroundTint, BLEND_Opaque);
        }
        else
        {
            DrawRect(Colors[Paving], X, Y, Width, Height);
        }
    }

    DrawStreetFinds(Center, CameraX, Scale);
    FountainToyBounds = FBox2D(ForceInit);
    FountainQuestion = FBox2D(ForceInit);

    const auto DrawPedestrian = [&](const FStreetPedestrian& Walker)
    {
        const FPedestrianCharacter& Character = PedestrianCharacters[Walker.Character];
        UTexture2D* Atlas = PedestrianAtlases[Walker.Character];
        constexpr int32 WalkPoses[] = {1, 0, 2, 0};
        const int32 Pose = Walker.PauseSeconds > 0 ? 0
            : WalkPoses[FMath::FloorToInt(Walker.WalkTime / Character.FrameSeconds) % UE_ARRAY_COUNT(WalkPoses)];
        const FVector2D Foot = Center + (Walker.Ground - FVector2D(CameraX, 0)) * Scale;
        const double Height = CharacterHeight * Character.HeightScale * Scale;
        constexpr double FrameWidth = 288.0;
        const double Width = Height * FrameWidth / Atlas->GetSizeY();
        const double FrameU = FrameWidth / Atlas->GetSizeX();
        DrawTexture(Atlas, Foot.X - Width * 0.5, Foot.Y - Height, Width, Height,
            (Pose + (Walker.bWalkRight ? 1 : 0)) * FrameU, 0, Walker.bWalkRight ? -FrameU : FrameU, 1,
            FMath::Lerp(GroundTint, FLinearColor::White, 0.18f), BLEND_Translucent);
    };
    const auto DrawProps = [&](bool bInFront)
    {
        TArray<const FStreetPedestrian*, TInlineAllocator<2>> Walkers;
        for (const FStreetPedestrian& Walker : StreetPedestrians)
        {
            if (!IsFallParticipant(Walker.Character) && !IsLostParticipant(Walker.Character) && (Walker.Ground.Y > Ground.Y) == bInFront)
            {
                Walkers.Add(&Walker);
            }
        }
        int32 NextWalker = 0;
        for (const AParkPropActor* Prop : ParkProps)
        {
            if (!IsValid(Prop) || Prop->ActorHasTag(EditorialFurnitureTag) || !Prop->Artwork ||
                (Prop->GetBase().Y > Ground.Y) != bInFront)
            {
                continue;
            }

            while (NextWalker < Walkers.Num() && Walkers[NextWalker]->Ground.Y < Prop->GetBase().Y)
            {
                DrawPedestrian(*Walkers[NextWalker++]);
            }
            const bool bUseBirdSafeArt = BirdSafeMallArt && BirdFixDay > 0 && GetGameDay() >= BirdFixDay &&
                Prop->Artwork->GetFName() == FName(TEXT("SP_RivermereMall"));
            UTexture2D* Texture = bUseBirdSafeArt ? BirdSafeMallArt.Get() : Prop->Artwork->GetBakedTexture();
            if (!Texture)
            {
                continue;
            }
            const FVector2D Base = Center + (Prop->GetBase() - FVector2D(CameraX, 0)) * Scale;
            const FVector2D Size = Prop->GetDrawSize() * Scale;
            DrawTexture(Texture, Base.X - Size.X * 0.5, Base.Y - Size.Y,
                Size.X, Size.Y, 0, 0, 1, 1, GroundTint, BLEND_Translucent);
            if (FountainToy && ToyPlace == EItemPlace::Ground && Prop->Artwork->GetFName() == FName(TEXT("SP_Fountain")))
            {
                const double Width = Size.X * 0.085;
                const double Height = Width * FountainToy->GetSizeY() / FountainToy->GetSizeX();
                const FVector2D ToyFoot = Base + FVector2D(Size.X * 0.24, -Size.Y * 0.305);
                const FVector2D Min = ToyFoot - FVector2D(Width * 0.5, Height);
                FountainToyBounds = FBox2D(Min, Min + FVector2D(Width, Height));
                DrawTexture(FountainToy, Min.X, Min.Y, Width, Height,
                    0, 0, 1, 1, GroundTint, BLEND_Translucent);
                const FVector2D Delta = Ground - Prop->GetBase();
                if (!bCameraMode && !bFountainReported && FMath::Abs(Delta.X) < 250 && FMath::Abs(Delta.Y) < 130)
                {
                    const FVector2D Marker = Min - FVector2D(0, 30 * Scale);
                    FountainQuestion = FBox2D(Marker, Marker + FVector2D(24, 24) * Scale);
                }
            }
        }
        while (NextWalker < Walkers.Num())
        {
            DrawPedestrian(*Walkers[NextWalker++]);
        }
    };
    DrawProps(false);
    DrawFallScene(Center, CameraX, Scale, GroundTint);
    DrawLostProperty(Center, CameraX, Scale, GroundTint);
    DrawBirdScene(Center, CameraX, Scale, GroundTint);
    const auto DrawRain = [&]()
    {
        if (!IsRainyDay()) { return; }
        const double Time = GetWorld()->GetTimeSeconds();
        const FLinearColor Rain(0.65f, 0.80f, 0.88f, 0.28f);
        for (int32 Index = 0; Index < 68; ++Index)
        {
            const double X = FMath::Frac(0.6180339 * Index) * Canvas->ClipX;
            const double Y = FMath::Frac(0.4142136 * Index + Time * (0.57 + (Index % 3) * 0.06)) * Canvas->ClipY;
            DrawLine(X, Y, X - 5 * Scale, Y + 17 * Scale, Rain, FMath::Max(1.0, Scale));
        }
    };

    if (!Reporter || !GetActivePoseAtlas() || bCameraMode || !PendingPhoto.IsEmpty())
    {
        DrawProps(true);
        DrawRain();
        DrawCameraOverlay();
        return;
    }

    DrawReporter(*Reporter, Center + FVector2D(Ground.X - CameraX, Ground.Y) * Scale,
        CharacterHeight * Scale, nullptr, FMath::Lerp(GroundTint, FLinearColor::White, 0.18f));
    DrawProps(true);
    DrawRain();
    DrawCameraOverlay();
    if (!bReporterPanelOpen)
    {
        for (const FStreetPedestrian& Walker : StreetPedestrians)
        {
            const double Height = CharacterHeight * PedestrianCharacters[Walker.Character].HeightScale;
            const FVector2D Head = Center + (Walker.Ground - FVector2D(CameraX, Height)) * Scale;
            if (!IsFallParticipant(Walker.Character) && !IsLostParticipant(Walker.Character) && Walker.SpeechSeconds > 0 && Head.X > 0 && Head.X < Canvas->ClipX)
            {
                DrawPedestrianSpeech(Walker.Speech, Head);
            }
        }
    }
    DrawStreetStatus();
    if (FountainQuestion.bIsValid)
    {
        const FVector2D Mark = FountainQuestion.Min;
        DrawRect(FLinearColor(0.12f, 0.24f, 0.22f, 0.95f), Mark.X, Mark.Y, 24 * Scale, 24 * Scale);
        DrawUiText(TEXT("?"), FLinearColor::White, Mark.X + 7 * Scale, Mark.Y + 3 * Scale,
            GEngine->GetMediumFont(), Scale);
    }
    const TPair<EInteraction, const TCHAR*> Entrances[] = {
        {EInteraction::Enter, TEXT("News Center")},
        {EInteraction::EnterSportsCenter, TEXT("Sports Center")},
        {EInteraction::EnterHotPot, TEXT("Hot Pot Restaurant")},
        {EInteraction::EnterMall, TEXT("Rivermere Mall")},
        {EInteraction::EnterHospital, TEXT("Hospital")}
    };
    for (const auto& Entry : Entrances)
    {
        double DoorX = 0.0;
        if (const AParkPropActor* Entrance = FindNearbyEntrance(Ground, Entry.Key, &DoorX))
        {
            Interaction = Entry.Key;
            DrawInteraction(Entry.Value, Center + FVector2D(DoorX - CameraX,
                Entrance->GetBase().Y - Entrance->GetDrawSize().Y * 0.25) * Scale);
            break;
        }
    }
    DrawFirstStoryGuide();
    DrawBackpack(Ground);
    DrawFallPrompts(Ground);
}

UTexture2D* AReporterHUD::GetActivePoseAtlas() const
{
    return bFemaleReporter && FemalePoseAtlas ? FemalePoseAtlas.Get() : PoseAtlas.Get();
}

void AReporterHUD::DrawReporter(const AReporterPawn& Reporter, FVector2D Foot, float Height,
    const FVector2D* FacingOverride, const FLinearColor& Tint)
{
    UTexture2D* Atlas = GetActivePoseAtlas();
    if (!Atlas)
    {
        return;
    }
    const EReporterFacing Facing = FacingOverride
        ? (FacingOverride->X != 0
            ? (FacingOverride->X < 0 ? EReporterFacing::Left : EReporterFacing::Right)
            : (FacingOverride->Y < 0 ? EReporterFacing::Back : EReporterFacing::Front))
        : Reporter.GetFacing();
    const bool bSide = Facing == EReporterFacing::Left || Facing == EReporterFacing::Right;
    const int32 Column = bSide ? 1 : (Facing == EReporterFacing::Back ? 2 : 0);
    const float Mirror = Facing == EReporterFacing::Right ? -1.0f : 1.0f;

    const bool bWalking = FacingOverride ? bIndoorWalking : Reporter.IsWalking();
    const int32 Beat = FMath::FloorToInt((FacingOverride ? FMath::Frac(GetWorld()->GetTimeSeconds() * 2.0f) : Reporter.GetWalkProgress()) * 4.0f);
    // Use the standing pose to bring the feet together between alternate contacts.
    const int32 Row = bWalking && Beat % 2 == 0 ? 1 + Beat / 2 : 0;
    const auto& ActivePoses = Atlas == FemalePoseAtlas ? FemalePoses : Poses;
    const FReporterPose& Stand = ActivePoses[0][Column];
    const FReporterPose& Pose = ActivePoses[Row][Column];
    const float WaistToFoot = Height * (Stand.Region.Max.Y - Stand.WaistY) / Stand.Region.Height();
    const float UpperScale = (Height - WaistToFoot) / (Pose.WaistY - Pose.Region.Min.Y);
    const float LowerScale = WaistToFoot / (Pose.Region.Max.Y - Pose.WaistY);
    const float WidthScale = Height / Pose.Region.Height();

    DrawSpriteMesh(*Canvas, *Atlas, Pose.Region, Tint,
        [&](const FVector2D& Pixel)
        {
            const double FromWaist = Pixel.Y - Pose.WaistY;
            const double X = (Pixel.X - Pose.CenterX) * WidthScale;
            const double Y = FromWaist * (FromWaist < 0.0 ? UpperScale : LowerScale) - WaistToFoot;
            return Foot + FVector2D(X * Mirror, Y);
        });
    if (HeldItem != EFoundItem::None && GetWorld()->GetRealTimeSeconds() < HeldItemUntil && Facing != EReporterFacing::Back)
    {
        const double Side = bSide ? -Mirror : 1.0;
        DrawFoundItem(HeldItem, Foot + FVector2D(Height * 0.19 * Side, -Height * 0.40), Height * 0.16);
    }
}

const AParkPropActor* AReporterHUD::FindNearbyEntrance(FVector2D Ground, EInteraction Entrance, double* DoorX) const
{
    FName Sprite;
    double DoorOffset = 0.0;
    const UTexture2D* Interior = nullptr;
    switch (Entrance)
    {
    case EInteraction::Enter:
        Sprite = FName(TEXT("SP_NewsCenter"));
        DoorOffset = 0.013;
        Interior = LobbyTexture;
        break;
    case EInteraction::EnterSportsCenter:
        Sprite = FName(TEXT("SP_SportsCenter"));
        Interior = SportsLobbyTexture;
        break;
    case EInteraction::EnterHotPot:
        Sprite = FName(TEXT("SP_RiversideHotPot"));
        DoorOffset = 0.14;
        Interior = HotPotTexture;
        break;
    case EInteraction::EnterMall:
        Sprite = FName(TEXT("SP_RivermereMall"));
        Interior = MallInterior;
        break;
    case EInteraction::EnterHospital:
        Sprite = FName(TEXT("SP_RivermereHospital"));
        Interior = HospitalInterior;
        break;
    default:
        return nullptr;
    }
    if (!Interior)
    {
        return nullptr;
    }

    for (const AParkPropActor* Prop : ParkProps)
    {
        if (!IsValid(Prop) || !Prop->Artwork || Prop->Artwork->GetFName() != Sprite)
        {
            continue;
        }
        const FVector2D Base = Prop->GetBase();
        const double X = Base.X + Prop->GetDrawSize().X * DoorOffset;
        if (FMath::Abs(Ground.X - X) <= 125.0 && Ground.Y >= Base.Y - 20.0 && Ground.Y <= Base.Y + 150.0)
        {
            if (DoorX)
            {
                *DoorX = X;
            }
            return Prop;
        }
    }
    return nullptr;
}

void AReporterHUD::DrawInteraction(const TCHAR* Label, FVector2D Marker)
{
    const float Scale = Canvas->ClipY / ReferenceHeight;
    const FVector2D ButtonMin(Canvas->ClipX - 192 * Scale, bInLobby ? Canvas->ClipY * 0.56 : Canvas->ClipY - 54 * Scale);
    const FVector2D ButtonSize(178 * Scale, 36 * Scale);
    InteractionButton = FBox2D(ButtonMin, ButtonMin + ButtonSize);
    const FVector2D Extent(11 * Scale, 11 * Scale);
    InteractionMarker = FBox2D(Marker - Extent * 1.5, Marker + Extent * 1.5);
    const FLinearColor Ink(0.12f, 0.08f, 0.15f, 0.93f);
    DrawRect(Ink, Marker.X - Extent.X, Marker.Y - Extent.Y, Extent.X * 2, Extent.Y * 2);
    DrawUiText(TEXT(">"), FLinearColor::White, Marker.X - 4 * Scale, Marker.Y - 7 * Scale,
        GEngine->GetSmallFont(), Scale);
    const FLinearColor PanelFill(0.96f, 0.88f, 0.72f);
    DrawRect(Ink, ButtonMin.X, ButtonMin.Y, ButtonSize.X, ButtonSize.Y);
    if (!bInLobby)
    {
        DrawRect(PanelFill, ButtonMin.X + 2 * Scale, ButtonMin.Y + 2 * Scale,
            ButtonSize.X - 4 * Scale, ButtonSize.Y - 4 * Scale);
    }
    DrawUiText(Label, bInLobby ? FLinearColor::White : Ink, ButtonMin.X + 12 * Scale, ButtonMin.Y + 9 * Scale,
        GEngine->GetSmallFont(), Scale * (bInLobby ? 1.12f : 1.44f));
}

void AReporterHUD::Interact(bool bUsePointer)
{
    if (bEndingOpen)
    {
        ClickEnding(bUsePointer);
        return;
    }
    if (bConfirmMorningRest)
    {
        ClickMorningRestConfirmation(bUsePointer);
        return;
    }
    if (bFurnitureShopOpen)
    {
        ClickFurnitureShop(bUsePointer);
        return;
    }
    if (bFoodShopOpen)
    {
        ClickFoodShop(bUsePointer);
        return;
    }
    if (bSettingsOpen || TitlePage == ETitlePage::Load)
    {
        float X, Y;
        if (bUsePointer && GetOwningPlayerController()->GetMousePosition(X, Y))
        {
            ClickSettings(FVector2D(X, Y));
        }
        return;
    }
    if (bUsePointer && !IsTitleOpen() && SettingsShortcut.bIsValid)
    {
        float X, Y;
        if (GetOwningPlayerController()->GetMousePosition(X, Y) && SettingsShortcut.IsInsideOrOn(FVector2D(X, Y)))
        {
            ToggleSettings();
            return;
        }
    }
    if (HotPotState != EHotPotState::Idle)
    {
        HandleHotPotClick(bUsePointer);
        return;
    }
    if (IsTitleOpen())
    {
        HandleTitleClick(bUsePointer);
        return;
    }
    if (SwimState != ESwimState::Idle)
    {
        HandleSwimClick(bUsePointer);
        return;
    }
    if (RunState != ERunState::Idle)
    {
        HandleTreadmillClick(bUsePointer);
        return;
    }
    if (bDesktopOpen)
    {
        float X, Y;
        if (bUsePointer && GetOwningPlayerController()->GetMousePosition(X, Y))
        {
            ClickDesktop(FVector2D(X, Y));
        }
        return;
    }
    if (bCameraMode)
    {
        TakePhoto();
        return;
    }
    if (bUsePointer)
    {
        float X, Y;
        if (GetOwningPlayerController()->GetMousePosition(X, Y) &&
            (ClickFallPrompts(FVector2D(X, Y)) || ClickBackpack(FVector2D(X, Y)) ||
                ClickStreetStatus(FVector2D(X, Y))))
        {
            return;
        }
    }
    if (bInEditorial)
    {
        float X, Y;
        if (bUsePointer && GetOwningPlayerController()->GetMousePosition(X, Y))
        {
            const FVector2D Pointer(X, Y);
            if (bNewsroomSetupGuide && SkipSetupGuideButton.bIsValid && SkipSetupGuideButton.IsInsideOrOn(Pointer))
            {
                bNewsroomSetupGuide = false;
                SkipSetupGuideButton = FBox2D(ForceInit);
                return;
            }
            if (ClickDecor(Pointer))
            {
                return;
            }
            if (!bReporterPanelOpen && MorningRestButton.bIsValid &&
                MorningRestButton.IsInsideOrOn(Pointer) && IsNearEditorialBed())
            {
                bConfirmMorningRest = true;
                MorningRestConfirm = MorningRestCancel = FBox2D(ForceInit);
                return;
            }
        }
        if (!bDecorating && bUsePointer && bHasDesk && DesktopWallpaper &&
            ComputerBounds.bIsValid && GetOwningPlayerController()->GetMousePosition(X, Y) &&
            ComputerBounds.IsInsideOrOn(FVector2D(X, Y)))
        {
            DesktopPage = INDEX_NONE;
            bReporterPanelOpen = false;
            bBackpackOpen = false;
            bDesktopOpen = true;
            return;
        }
        if (bDecorating)
        {
            if (!bUsePointer)
            {
                FinishDecorating();
            }
            return;
        }
    }
    if (bReporterPanelOpen)
    {
        return;
    }
    if (!bInLobby && bUsePointer && FountainQuestion.bIsValid)
    {
        float X, Y;
        if (GetOwningPlayerController()->GetMousePosition(X, Y) && FountainQuestion.IsInsideOrOn(FVector2D(X, Y)))
        {
            bFountainDiscovered = true;
            ToggleCamera();
            return;
        }
    }
    if (Interaction == EInteraction::None)
    {
        return;
    }
    if (bUsePointer)
    {
        float X, Y;
        if (!GetOwningPlayerController()->GetMousePosition(X, Y) ||
            (!InteractionButton.IsInsideOrOn(FVector2D(X, Y)) &&
             !InteractionMarker.IsInsideOrOn(FVector2D(X, Y))))
        {
            return;
        }
    }

    switch (Interaction)
    {
    case EInteraction::Enter:
    case EInteraction::EnterSportsCenter:
    case EInteraction::EnterHotPot:
    case EInteraction::EnterMall:
    case EInteraction::EnterHospital:
    {
        const bool bSportsCenter = Interaction == EInteraction::EnterSportsCenter;
        if (const AReporterPawn* Reporter = Cast<AReporterPawn>(GetOwningPawn());
            Reporter && FindNearbyEntrance(Reporter->GetGroundPosition(), Interaction))
        {
            bInLobby = true;
            LeaveFallSceneForBuilding();
            bInSportsCenter = bSportsCenter;
            bInHotPot = Interaction == EInteraction::EnterHotPot;
            bInMall = Interaction == EInteraction::EnterMall;
            bInHospital = Interaction == EInteraction::EnterHospital;
            if (bInMall) { MallPosition = FVector2D(0.5, 0.92); }
            bUpstairs = false;
            LobbyX = bSportsCenter || bInHospital ? 0.50 : 0.10;
            IndoorDirection = FVector2D(1, 0);
        }
        break;
    }
    case EInteraction::TreatHealth:
        TreatHealth();
        break;
    case EInteraction::BrowseFood:
        OpenFoodShop();
        break;
    case EInteraction::BrowseFurniture:
        bFurnitureShopOpen = true;
        bBackpackOpen = false;
        ShopBuy = ShopClose = FBox2D(ForceInit);
        for (FBox2D& Tab : ShopTabs) { Tab = FBox2D(ForceInit); }
        for (FBox2D& Card : ShopCards) { Card = FBox2D(ForceInit); }
        break;
    case EInteraction::OrderHotPot:
        HotPotState = EHotPotState::Order;
        break;
    case EInteraction::StartRun:
        StartTreadmillRun();
        break;
    case EInteraction::EnterGym:
        if (bInSportsCenter && GymTexture)
        {
            bInGym = true;
            LobbyX = 0.91;
            IndoorDirection = FVector2D(-1, 0);
        }
        break;
    case EInteraction::EnterPool:
        if (bInSportsCenter && PoolTexture)
        {
            bInPool = true;
            PoolPosition = FVector2D(0.095, 0.60);
            IndoorDirection = FVector2D(0, 1);
        }
        break;
    case EInteraction::StartSwim:
        StartSwimRace();
        break;
    case EInteraction::ExitPool:
        bInPool = false;
        LobbyX = 0.82;
        IndoorDirection = FVector2D(-1, 0);
        break;
    case EInteraction::ExitGym:
        bInGym = false;
        LobbyX = 0.18;
        IndoorDirection = FVector2D(1, 0);
        break;
    case EInteraction::Exit:
        bInLobby = false;
        bRecheckPedestriansOnStreet = true;
        bInSportsCenter = false;
        bInHotPot = false;
        bInMall = false;
        bInHospital = false;
        break;
    case EInteraction::Reception:
        bShowGuide = !bShowGuide;
        break;
    case EInteraction::Stairs:
        StairProgress = 0.0;
        break;
    case EInteraction::Editorial:
        if (EditorialTexture)
        {
            bInEditorial = true;
            EditorialPosition = FVector2D(0.89, 0.79);
            IndoorDirection = FVector2D(-1, 0);
        }
        break;
    case EInteraction::RestAtBed:
        RestAtEditorialBed();
        break;
    case EInteraction::ExitEditorial:
        bInEditorial = false;
        LobbyX = 0.50;
        break;
    case EInteraction::Locked:
        LobbyNotice = TEXT("Room locked");
        LobbyNoticeUntil = GetWorld()->GetRealTimeSeconds() + 2.0;
        break;
    default:
        break;
    }
    Interaction = EInteraction::None;
    InteractionButton = FBox2D(ForceInit);
    InteractionMarker = FBox2D(ForceInit);
}

void AReporterHUD::DrawDesktop()
{
    static const TCHAR* Labels[] = {TEXT("Mail"), TEXT("Editor"), TEXT("Newsroom")};
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const double ScreenScale = IsValid(DesktopSettings)
        ? FMath::Clamp(DesktopSettings->ScreenScale, 0.5f, 1.0f) : 0.93;
    const double Height = FMath::Min(View.Y, View.X * 0.75) * ScreenScale;
    const FVector2D Size(Height * 4 / 3, Height);
    const FVector2D Origin = (View - Size) * 0.5;
    const float S = Height / 600;
    const auto At = [&](double X, double Y) { return Origin + Size * FVector2D(X, Y); };
    DrawRect(FLinearColor(0.024f, 0.030f, 0.035f), 0, 0, View.X, View.Y);
    DrawTexture(DesktopWallpaper, Origin.X, Origin.Y, Size.X, Size.Y,
        0, 0, 1, 1, FLinearColor::White, BLEND_Opaque);

    const FVector2D Close = At(0.944, 0.023);
    DesktopClose = FBox2D(Close, Close + FVector2D(32, 27) * S);
    if (DesktopPage == 0)
    {
        DrawMail(Origin, Size, Close);
        return;
    }
    if (DesktopPage == 1)
    {
        DrawStoryEditor(Origin, Size, Close, FVector2D(32, 27) * S);
        return;
    }

    DrawRect(FLinearColor(0.26f, 0.17f, 0.13f), Close.X, Close.Y, 32 * S, 27 * S);
    DrawUiText(TEXT("X"), FLinearColor::White, Close.X + 11 * S, Close.Y + 5 * S, GEngine->GetSmallFont(), S);
    if (DesktopPage == 2)
    {
        DrawNewsroom(Origin, S, Close);
        return;
    }

    float X = 0, Y = 0;
    const bool bHasMouse = GetOwningPlayerController()->GetMousePosition(X, Y);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Labels); ++Index)
    {
        const FVector2D Center = At(0.25 + Index * 0.25, 0.42);
        DesktopButtons[Index] = FBox2D(Center - FVector2D(65, 56) * S, Center + FVector2D(65, 70) * S);
        if (bHasMouse && DesktopButtons[Index].IsInsideOrOn(FVector2D(X, Y)))
        {
            const FBox2D& Box = DesktopButtons[Index];
            DrawRect(FLinearColor(0.86f, 0.83f, 0.60f, 0.18f), Box.Min.X, Box.Min.Y,
                Box.GetSize().X, Box.GetSize().Y);
        }
        if (UTexture2D* Icon = DesktopIcons[Index])
        {
            const double Fit = 93 * S / FMath::Max(Icon->GetSizeX(), Icon->GetSizeY());
            const FVector2D IconSize = FVector2D(Icon->GetSizeX(), Icon->GetSizeY()) * Fit;
            DrawTexture(Icon, Center.X - IconSize.X * 0.5, Center.Y + 36 * S - IconSize.Y,
                IconSize.X, IconSize.Y, 0, 0, 1, 1, FLinearColor::White, BLEND_Translucent);
            if (Index == 0 && HasUnreadMail())
            {
                DrawMailUnreadDot(Center + FVector2D(IconSize.X * 0.44, 36 * S - IconSize.Y), 6 * S);
            }
        }
        float LabelWidth = 0, LabelHeight = 0;
        MeasureUiText(Labels[Index], LabelWidth, LabelHeight, GEngine->GetMediumFont(), S);
        DrawUiText(Labels[Index], FLinearColor(1.0f, 0.91f, 0.73f),
            Center.X - LabelWidth * 0.5, Center.Y + 45 * S, GEngine->GetMediumFont(), S);
    }
    const TCHAR* DesktopHint = nullptr;
    if (bNewsroomSetupGuide && (HasUnreadMail() || !bSponsorshipClaimed))
    {
        DesktopHint = TEXT("Open Mail to check your inbox.");
    }
    else if (bFirstStoryGuide && !LatestCapturedFountainPhoto.IsEmpty())
    {
        DesktopHint = TEXT("Open Editor to write your fountain story.");
    }
    if (DesktopHint)
    {
        DrawRect(FLinearColor(0.07f, 0.24f, 0.22f), Origin.X + 30 * S, Origin.Y + 30 * S, 382 * S, 36 * S);
        DrawUiText(DesktopHint, FLinearColor(1.0f, 0.91f, 0.73f),
            Origin.X + 42 * S, Origin.Y + 39 * S, GEngine->GetMediumFont(), S);
    }
}

bool AReporterHUD::ClickDesktop(FVector2D Mouse)
{
    if (DesktopClose.bIsValid && DesktopClose.IsInsideOrOn(Mouse))
    {
        HideStoryInputs();
        if (DesktopPage == 2)
        {
            bShowFirstStoryResults = false;
        }
        if (DesktopPage == INDEX_NONE)
        {
            bDesktopOpen = false;
        }
        DesktopPage = INDEX_NONE;
        return true;
    }
    if (DesktopPage == 0)
    {
        return ClickMail(Mouse);
    }
    if (DesktopPage == 2)
    {
        return ClickNewsroom(Mouse);
    }
    if (DesktopPage == 1 && ClickStoryEditor(Mouse))
    {
        return true;
    }
    if (DesktopPage == INDEX_NONE)
    {
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(DesktopButtons); ++Index)
        {
            if (DesktopButtons[Index].bIsValid && DesktopButtons[Index].IsInsideOrOn(Mouse))
            {
                DesktopPage = Index;
                if (Index == 1)
                {
                    RefreshEditorPhotos();
                    if (StoryKind == 2 && FallCases.IsValidIndex(StoryFallCase))
                    {
                        ApplyFallTemplate();
                    }
                    else if (StoryKind == 3)
                    {
                        ApplyWalletTemplate();
                    }
                }
                return true;
            }
        }
    }
    return false;
}

void AReporterHUD::DrawEditorial(const AReporterPawn& Reporter)
{
    if (bDesktopOpen)
    {
        DrawDesktop();
        return;
    }
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const double Aspect = double(EditorialTexture->GetSizeX()) / EditorialTexture->GetSizeY();
    const double Height = FMath::Min(View.Y, View.X / Aspect);
    const FVector2D Size(Height * Aspect, Height);
    const FVector2D TopLeft = (View - Size) * 0.5;
    DrawRect(FLinearColor(0.07f, 0.045f, 0.05f), 0, 0, View.X, View.Y);
    DrawTexture(EditorialTexture, TopLeft.X, TopLeft.Y, Size.X, Size.Y,
        0, 0, 1, 1, FLinearColor::White, BLEND_Opaque);

    DecorRoomOrigin = TopLeft;
    DecorRoomSize = Size;
    if (bDraggingDesk || bDraggingComputer || bDraggingBed || bRotatingBed)
    {
        UpdateDeskDrag();
    }
    else if (bDecorating && (bBedSelected ? bHasBed : bHasDesk))
    {
        const FVector2D Move = Reporter.GetMoveInput().GetClampedToMaxSize(1.0);
        const FVector2D Step = Move * FVector2D(0.22, 0.24) * GetWorld()->GetDeltaSeconds();
        if (bBedSelected)
        {
            BedPosition += Step;
            ConstrainBed();
        }
        else
        {
            DeskPosition += Step;
            ConstrainDesk();
        }
    }

    const FVector2D Input = bDecorating ? FVector2D::ZeroVector : Reporter.GetMoveInput().GetClampedToMaxSize(1.0);
    const FVector2D Previous = EditorialPosition;
    EditorialPosition += Input * FVector2D(0.22, 0.24) * GetWorld()->GetDeltaSeconds();
    EditorialPosition.X = FMath::Clamp(EditorialPosition.X, 0.035, 0.965);

    // The walkable floor follows the back wall and both sloped side walls in the artwork.
    const double BackEdge = EditorialBackEdge(EditorialPosition.X);
    EditorialPosition.Y = FMath::Clamp(EditorialPosition.Y, BackEdge, 0.97);
    DeskBounds = GetDeskBounds(TopLeft, Size);
    const auto DrawDesk = [&](bool bInFront)
    {
        if (!bHasDesk || (DeskPosition.Y > EditorialPosition.Y) != bInFront)
        {
            return;
        }
        const FFurnitureView Art = GetFurnitureView(EFurniture::Desk, EquippedFurniture.Y, DeskTurn);
        if (Art.Texture)
        {
            DrawFurnitureView(Art, DeskBounds, DeskTurn == 3);
            DrawComputer();
        }
    };
    if (bHasDesk && !bDecorating)
    {
        const bool bSide = DeskTurn % 2 != 0;
        const FVector2D Radius((bSide ? 79.0 : 125.0) / EditorialWidth,
            (bSide ? 47.0 : 28.0) / EditorialHeight);
        const FVector2D ScaledRadius = Radius * DeskScale;
        const FVector2D Center = DeskPosition - FVector2D(0, ScaledRadius.Y);
        const auto Blocks = [&](FVector2D Position)
        {
            return ((Position - Center) / ScaledRadius).SizeSquared() < 1.0;
        };
        if (Blocks(EditorialPosition) && !Blocks(Previous))
        {
            EditorialPosition = Previous;
        }
    }
    bIndoorWalking = !Previous.Equals(EditorialPosition, UE_SMALL_NUMBER);
    if (!Input.IsNearlyZero())
    {
        IndoorDirection = Input.X != 0.0 ? FVector2D(Input.X, 0.0) : FVector2D(0.0, Input.Y);
    }

    const FFurnitureView Bed = bHasBed ? GetFurnitureView(EFurniture::Bed, EquippedFurniture.X, BedTurn) : FFurnitureView{};
    BedBounds = FBox2D(ForceInit);
    if (Bed.Texture)
    {
        const FVector2D Foot = TopLeft + BedPosition * Size;
        const double BedHeight = 220.0 * BedScale / EditorialHeight * Size.Y;
        const double BedWidth = BedHeight * Bed.Aspect;
        BedBounds = FBox2D(Foot - FVector2D(BedWidth * 0.5, BedHeight),
            Foot + FVector2D(BedWidth * 0.5, 0));
        if (!bDecorating)
        {
            const FVector2D Radius = GetBedRadius();
            const FVector2D Center = BedPosition - FVector2D(0, Radius.Y);
            if (((EditorialPosition - Center) / Radius).SizeSquared() < 1.0 &&
                ((Previous - Center) / Radius).SizeSquared() >= 1.0)
            {
                EditorialPosition = Previous;
                bIndoorWalking = false;
            }
        }
    }
    const auto DrawBed = [&]()
    {
        DrawFurnitureView(Bed, BedBounds, BedTurn == 3, BedAngle);
    };
    const auto DrawFurniture = [&](bool bInFront)
    {
        const bool bDrawBed = (BedPosition.Y > EditorialPosition.Y) == bInFront;
        if (bDrawBed && BedPosition.Y <= DeskPosition.Y) { DrawBed(); }
        DrawDesk(bInFront);
        if (bDrawBed && BedPosition.Y > DeskPosition.Y) { DrawBed(); }
    };
    DrawFurniture(false);
    const double Depth = (EditorialPosition.Y - 0.674) / (0.97 - 0.674);
    DrawReporter(Reporter, TopLeft + Size * EditorialPosition,
        Height * FMath::Lerp(0.25, 0.32, Depth), &IndoorDirection);
    DrawFurniture(true);
    BedRotationHandle = FBox2D(ForceInit);
    if (bDecorating && bBedSelected && BedBounds.bIsValid)
    {
        const double Scale = Height / EditorialHeight;
        const FVector2D Pivot = BedBounds.GetCenter();
        const FVector2D Arm = FVector2D(0, -BedBounds.GetSize().Y * 0.5 - 32 * Scale).GetRotated(BedAngle);
        const FVector2D Handle = Pivot + Arm;
        const FVector2D Radius(14 * Scale);
        BedRotationHandle = FBox2D(Handle - Radius, Handle + Radius);
        const FVector2D Stem = Handle - Arm.GetSafeNormal() * 14 * Scale;
        const FVector2D Edge = Pivot + FVector2D(0, -BedBounds.GetSize().Y * 0.5).GetRotated(BedAngle);
        DrawLine(Edge.X, Edge.Y, Stem.X, Stem.Y, FLinearColor::White, 2.0f);
        DrawRect(FLinearColor(0.18f, 0.12f, 0.16f, 0.96f), Handle.X - Radius.X,
            Handle.Y - Radius.Y, 2 * Radius.X, 2 * Radius.Y);
        DrawUiText(TEXT("R"), FLinearColor::White, Handle.X - 5 * Scale, Handle.Y - 9 * Scale,
            GEngine->GetSmallFont(), Scale);
    }
    DrawDecorControls();
    DrawNewsroomSetupGuide(TopLeft, Size);
    if (bDecorating)
    {
        return;
    }

    const FVector2D DoorApproach(0.92, 0.78);
    if (FMath::Abs(EditorialPosition.X - DoorApproach.X) <= 0.08 &&
        FMath::Abs(EditorialPosition.Y - DoorApproach.Y) <= 0.08)
    {
        Interaction = EInteraction::ExitEditorial;
        DrawInteraction(TEXT("Back to Lobby"), TopLeft + Size * FVector2D(0.94, 0.55));
    }
    else if (IsNearEditorialBed())
    {
        DrawBedRestAction(TopLeft, Size);
    }
}

FBox2D AReporterHUD::GetDeskBounds(FVector2D TopLeft, FVector2D Size) const
{
    const double Width = (DeskTurn % 2 ? DeskSideWidth : DeskWidth) * DeskScale / EditorialWidth * Size.X;
    const double Height = DeskHeight * DeskScale / EditorialHeight * Size.Y;
    const FVector2D Foot = TopLeft + DeskPosition * Size;
    return FBox2D(Foot - FVector2D(Width * 0.5, Height), Foot + FVector2D(Width * 0.5, 0));
}

void AReporterHUD::ConstrainDesk()
{
    const bool bSide = DeskTurn % 2 != 0;
    const double HalfWidth = (bSide ? DeskSideWidth : DeskWidth) * 0.5 * DeskScale / EditorialWidth;
    DeskPosition.X = FMath::Clamp(DeskPosition.X, 0.04 + HalfWidth, 0.96 - HalfWidth);
    const double Back = FMath::Max(EditorialBackEdge(DeskPosition.X - HalfWidth),
        EditorialBackEdge(DeskPosition.X + HalfWidth));
    DeskPosition.Y = FMath::Clamp(DeskPosition.Y, Back + (bSide ? 0.095 : 0.060) * DeskScale, 0.95);
}

FVector2D AReporterHUD::GetComputerTravel() const
{
    const FVector2D DeskSize = DeskBounds.GetSize();
    return FVector2D(DeskSize.X * 0.34, DeskSize.Y * 0.12);
}

FVector2D AReporterHUD::GetComputerFoot() const
{
    const FVector2D DeskSize = DeskBounds.GetSize();
    return FVector2D(DeskBounds.GetCenter().X, DeskBounds.Min.Y + DeskSize.Y * 0.24)
        + RotateTablePoint(ComputerPosition, DeskTurn) * GetComputerTravel();
}

void AReporterHUD::ConstrainComputer()
{
    // Reserve clearance for the computer's base within the painted desktop.
    const bool bSide = DeskTurn % 2 != 0;
    const double SideClearance = FMath::Clamp(1.0 - (bSide ? 0.54 : 0.34) * ComputerScale / DeskScale, 0.0, 1.0);
    const double DepthClearance = FMath::Clamp(1.0 - 0.48 * ComputerScale / DeskScale, 0.0, 1.0);
    FVector2D Projected = RotateTablePoint(ComputerPosition, DeskTurn);
    Projected.X = FMath::Clamp(Projected.X, -SideClearance, SideClearance);
    Projected.Y = FMath::Clamp(Projected.Y, -DepthClearance, DepthClearance);
    ComputerPosition = RotateTablePoint(Projected, (4 - DeskTurn) % 4);
}

void AReporterHUD::DrawComputer()
{
    const FFurnitureView Art = GetFurnitureView(EFurniture::Computer, EquippedFurniture.Z, DeskTurn);
    if (!Art.Texture)
    {
        ComputerBounds = FBox2D(ForceInit);
        return;
    }
    const FVector2D Foot = GetComputerFoot();
    const double DeskHeightOnScreen = DeskBounds.GetSize().Y;
    const double Height = DeskHeightOnScreen * 0.54 * ComputerScale;
    const double Width = Height * Art.Aspect;
    ComputerBounds = FBox2D(Foot - FVector2D(Width * 0.5, Height), Foot + FVector2D(Width * 0.5, 0));
    DrawFurnitureView(Art, ComputerBounds, DeskTurn == 3);
}

FVector2D AReporterHUD::GetBedRadius() const
{
    const bool bSide = BedTurn % 2 != 0;
    return FVector2D((bSide ? 166.0 : 103.0) / EditorialWidth,
        (bSide ? 36.0 : 61.0) / EditorialHeight) * BedScale;
}

void AReporterHUD::ConstrainBed()
{
    const FVector2D Radius = GetBedRadius();
    BedPosition.X = FMath::Clamp(BedPosition.X, 0.04 + Radius.X, 0.96 - Radius.X);
    const double Back = FMath::Max(EditorialBackEdge(BedPosition.X - Radius.X),
        EditorialBackEdge(BedPosition.X + Radius.X));
    BedPosition.Y = FMath::Clamp(BedPosition.Y, Back + 2.0 * Radius.Y, 0.96);
}

void AReporterHUD::DrawDecorControls()
{
    const double Scale = GetHudScale();
    const FVector2D Start(14 * Scale, (bReporterPanelOpen ? 327 : 95) * Scale);
    const auto Button = [&](FBox2D& Bounds, const TCHAR* Label, double Offset, double Width, bool bEnabled)
    {
        const FVector2D Min = Start + FVector2D(Offset, 0) * Scale;
        const FVector2D Extent = FVector2D(Width, 34) * Scale;
        Bounds = bEnabled ? FBox2D(Min, Min + Extent) : FBox2D(ForceInit);
        DrawRect(bEnabled ? FLinearColor(0.15f, 0.09f, 0.12f, 0.96f)
                          : FLinearColor(0.24f, 0.20f, 0.21f, 0.86f),
            Min.X, Min.Y, Extent.X, Extent.Y);
        DrawUiText(Label, bEnabled ? FLinearColor::White : FLinearColor(0.65f, 0.65f, 0.65f),
            Min.X + 10 * Scale, Min.Y + 9 * Scale, GEngine->GetSmallFont(), Scale * 1.1f);
    };
    if (!bDecorating)
    {
        Button(DecorButton, TEXT("Decorate [E]"), 0, 102, true);
        return;
    }

    Button(DeskButton, TEXT("Desk >"), 0, 102, true);
    Button(BedButton, TEXT("Bed >"), 356, 100, BedViews[0] && BedViews[1] && BedViews[2]);
    Button(ComputerFurnitureButton, TEXT("Computer >"), 464, 122, bHasDesk);
    const int32 Angle = bBedSelected
        ? (FMath::RoundToInt(BedTurn * 90 + BedAngle) % 360 + 360) % 360 : DeskTurn * 90;
    const FString Rotation = FString::Printf(TEXT("Rotate [R] %d"), Angle);
    Button(RotateButton, *Rotation, 110, 118, bBedSelected ? bHasBed : bHasDesk);
    Button(DoneButton, TEXT("Done [Enter]"), 236, 112, true);
    const FString Selected = FString::Printf(TEXT("%s  |  %s  |  %s"), *FurnitureLabel(EFurniture::Desk),
        *FurnitureLabel(EFurniture::Computer), *FurnitureLabel(EFurniture::Bed));
    DrawUiText(DecorStatus.IsEmpty() ? Selected : DecorStatus,
        FLinearColor::White, Start.X, Start.Y + 44 * Scale, GEngine->GetSmallFont(), Scale);
}

bool AReporterHUD::ClickDecor(FVector2D Mouse)
{
    if (!bDecorating)
    {
        if (!DecorButton.IsInsideOrOn(Mouse))
        {
            return false;
        }
        BeginDecorating();
        return true;
    }
    if (DoneButton.IsInsideOrOn(Mouse))
    {
        FinishDecorating();
    }
    else if (DeskButton.IsInsideOrOn(Mouse))
    {
        CycleFurniture(EFurniture::Desk);
    }
    else if (BedButton.bIsValid && BedButton.IsInsideOrOn(Mouse))
    {
        CycleFurniture(EFurniture::Bed);
    }
    else if (ComputerFurnitureButton.bIsValid && ComputerFurnitureButton.IsInsideOrOn(Mouse))
    {
        CycleFurniture(EFurniture::Computer);
    }
    else if (RotateButton.IsInsideOrOn(Mouse))
    {
        RotateDesk();
    }
    else if (BedRotationHandle.bIsValid && BedRotationHandle.IsInsideOrOn(Mouse))
    {
        const FVector2D Delta = Mouse - BedBounds.GetCenter();
        PointerAngleAtGrab = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
        BedAngleAtGrab = BedAngle;
        bRotatingBed = true;
    }
    else if (BedBounds.bIsValid && BedBounds.IsInsideOrOn(BedBounds.GetCenter() +
        (Mouse - BedBounds.GetCenter()).GetRotated(-BedAngle)) &&
        (!bHasDesk || BedPosition.Y >= DeskPosition.Y ||
            (!DeskBounds.IsInsideOrOn(Mouse) && !ComputerBounds.IsInsideOrOn(Mouse))))
    {
        bBedSelected = true;
        const FVector2D Foot(BedBounds.GetCenter().X, BedBounds.Max.Y);
        DragOffset = (Foot - Mouse) / DecorRoomSize;
        bDraggingBed = true;
    }
    else if (bHasDesk && ComputerBounds.bIsValid && ComputerBounds.IsInsideOrOn(Mouse))
    {
        bBedSelected = false;
        ComputerDragOffset = GetComputerFoot() - Mouse;
        bDraggingComputer = true;
    }
    else if (bHasDesk && DeskBounds.IsInsideOrOn(Mouse))
    {
        bBedSelected = false;
        const FVector2D Foot(DeskBounds.GetCenter().X, DeskBounds.Max.Y);
        DragOffset = (Foot - Mouse) / DecorRoomSize;
        bDraggingDesk = true;
    }
    return true;
}

void AReporterHUD::UpdateDeskDrag()
{
    float X, Y;
    if (APlayerController* Player = GetOwningPlayerController();
        Player && Player->GetMousePosition(X, Y))
    {
        if (bRotatingBed)
        {
            const FVector2D Delta = FVector2D(X, Y) - BedBounds.GetCenter();
            if (!Delta.IsNearlyZero())
            {
                const double PointerAngle = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
                BedAngle = FMath::Fmod(BedAngleAtGrab + PointerAngle - PointerAngleAtGrab, 360.0);
            }
        }
        else if (bDraggingBed)
        {
            BedPosition = (FVector2D(X, Y) - DecorRoomOrigin) / DecorRoomSize + DragOffset;
            ConstrainBed();
        }
        else if (bDraggingComputer)
        {
            const FVector2D Travel = GetComputerTravel();
            const FVector2D Center = GetComputerFoot()
                - RotateTablePoint(ComputerPosition, DeskTurn) * Travel;
            const FVector2D Offset = (FVector2D(X, Y) + ComputerDragOffset - Center) / Travel;
            ComputerPosition = RotateTablePoint(Offset, (4 - DeskTurn) % 4);
            ConstrainComputer();
        }
        else
        {
            DeskPosition = (FVector2D(X, Y) - DecorRoomOrigin) / DecorRoomSize + DragOffset;
            ConstrainDesk();
        }
    }
}

void AReporterHUD::ReleasePointer()
{
    if (bEndingOpen) { return; }
    if (bSettingsOpen || TitlePage == ETitlePage::Load)
    {
        if (SettingsSlider != ESettingsAction::Close)
        {
            float X, Y;
            if (GetOwningPlayerController()->GetMousePosition(X, Y)) { UpdateSettingsSlider(X); }
            SettingsSlider = ESettingsAction::Close;
            StoreAudioSettings();
        }
        return;
    }
    if (CropDrag)
    {
        UpdateCrop();
        CropDrag = 0;
    }
    if (bDraggingDesk || bDraggingComputer || bDraggingBed || bRotatingBed)
    {
        UpdateDeskDrag();
        bRotatingBed = false;
        bDraggingBed = false;
        bDraggingDesk = false;
        bDraggingComputer = false;
    }
}

void AReporterHUD::DecorShortcut(FKey Key)
{
    if (bEndingOpen || bConfirmMorningRest || !bInEditorial || bDesktopOpen)
    {
        return;
    }
    if (Key == EKeys::E && !bDecorating)
    {
        BeginDecorating();
    }
    else if (bDecorating && Key == EKeys::F)
    {
        AddDesk();
    }
    else if (bDecorating && Key == EKeys::R)
    {
        RotateDesk();
    }
}

void AReporterHUD::BeginDecorating()
{
    if (DeskViews.Num() == 3 && DeskViews[0] && DeskViews[1] && DeskViews[2])
    {
        bDecorating = true;
        DecorStatus.Reset();
    }
}

void AReporterHUD::AddDesk()
{
    bBedSelected = false;
    if (!bHasDesk)
    {
        bHasDesk = true;
        ConstrainDesk();
    }
}

void AReporterHUD::RotateDesk()
{
    if (bBedSelected && bHasBed)
    {
        BedAngle = 0.0;
        BedTurn = (BedTurn + 1) % 4;
        ConstrainBed();
    }
    else if (bHasDesk)
    {
        DeskTurn = (DeskTurn + 1) % 4;
        ConstrainDesk();
        ConstrainComputer();
    }
}

void AReporterHUD::FinishDecorating()
{
    if (bHasDesk || bHasBed)
    {
        const FString Contents = FString::Printf(TEXT("%.8f,%.8f,%d,%.4f,%.4f,%.8f,%.8f,%d,%d,%.8f,%.8f,%d,%.4f,%.8f"),
            DeskPosition.X, DeskPosition.Y, DeskTurn, DeskScale, ComputerScale, ComputerPosition.X, ComputerPosition.Y,
            int32(bHasDesk), int32(bHasBed), BedPosition.X, BedPosition.Y, BedTurn, BedScale, BedAngle);
        if (!FFileHelper::SaveStringToFile(Contents, *(FPaths::ProjectSavedDir() / TEXT("EditorialDesk.txt"))))
        {
            DecorStatus = TEXT("Could not save layout");
            return;
        }
    }
    bDraggingDesk = false;
    bDraggingComputer = false;
    bDraggingBed = false;
    bRotatingBed = false;
    bDecorating = false;
}

void AReporterHUD::DrawHotPotInterior(const AReporterPawn& Reporter)
{
    if (HotPotState != EHotPotState::Idle)
    {
        DrawHotPotChallenge();
        return;
    }
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const double Aspect = double(HotPotTexture->GetSizeX()) / HotPotTexture->GetSizeY();
    const double Height = FMath::Min(View.Y, View.X / Aspect);
    const FVector2D Size(Height * Aspect, Height);
    const FVector2D TopLeft = (View - Size) * 0.5;
    DrawRect(FLinearColor(0.07f, 0.045f, 0.05f), 0, 0, View.X, View.Y);
    DrawTexture(HotPotTexture, TopLeft.X, TopLeft.Y, Size.X, Size.Y,
        0, 0, 1, 1, FLinearColor::White, BLEND_Opaque);

    const double InputX = Reporter.GetMoveInput().X;
    const double Previous = LobbyX;
    LobbyX = FMath::Clamp(LobbyX + InputX * GetWorld()->GetDeltaSeconds() * 0.22, 0.05, 0.95);
    bIndoorWalking = !FMath::IsNearlyEqual(Previous, LobbyX);
    if (!FMath::IsNearlyZero(InputX))
    {
        IndoorDirection.X = InputX;
    }
    DrawReporter(Reporter, TopLeft + Size * FVector2D(LobbyX, 0.825), Height * 0.36, &IndoorDirection);

    if (LobbyX < 0.16)
    {
        Interaction = EInteraction::Exit;
        DrawInteraction(TEXT("Exit to Street"), TopLeft + Size * FVector2D(0.093, 0.53));
    }
    else if (HotPotTable && HotPotChopsticks && LobbyX >= 0.19 && LobbyX <= 0.40)
    {
        Interaction = EInteraction::OrderHotPot;
        DrawInteraction(TEXT("Order Hot Pot"), TopLeft + Size * FVector2D(0.28, 0.47));
    }
}

void AReporterHUD::DrawSportsLobby(const AReporterPawn& Reporter)
{
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    UTexture2D* RoomTexture = bInPool ? PoolTexture.Get() : (bInGym ? GymTexture.Get() : SportsLobbyTexture.Get());
    const double Aspect = double(RoomTexture->GetSizeX()) / RoomTexture->GetSizeY();
    const double Height = FMath::Min(View.Y, View.X / Aspect);
    const FVector2D Size(Height * Aspect, Height);
    const FVector2D TopLeft = (View - Size) * 0.5;
    DrawRect(FLinearColor(0.07f, 0.045f, 0.05f), 0, 0, View.X, View.Y);
    DrawTexture(RoomTexture, TopLeft.X, TopLeft.Y, Size.X, Size.Y,
        0, 0, 1, 1, FLinearColor::White, BLEND_Opaque);

    if (bInPool)
    {
        if (SwimState == ESwimState::Idle)
        {
            DrawPoolDeck(Reporter, TopLeft, Size);
        }
        else
        {
            DrawSwimRace(TopLeft, Size);
        }
        return;
    }

    FBox2D TreadmillBounds(ForceInit);
    if (bInGym)
    {
        const double UnitsToScreen = Height / RoomTexture->GetSizeY();
        for (const AParkPropActor* Prop : GymProps)
        {
            if (!IsValid(Prop) || !Prop->Artwork)
            {
                continue;
            }
            if (UTexture2D* Texture = Prop->Artwork->GetBakedTexture())
            {
                const FVector2D Foot = TopLeft + (Prop->GetBase() - GymLayoutOrigin) * UnitsToScreen;
                const FVector2D Extent = Prop->GetDrawSize() * UnitsToScreen;
                if (Prop->Artwork->GetFName() == FName(TEXT("SP_GymTreadmill")))
                {
                    const FVector2D Min = Foot - FVector2D(Extent.X * 0.5, Extent.Y);
                    TreadmillBounds = FBox2D(Min, Min + Extent);
                    RunFoot = Min + Extent * FVector2D(0.43, 0.77);
                }
                DrawTexture(Texture, Foot.X - Extent.X * 0.5, Foot.Y - Extent.Y,
                    Extent.X, Extent.Y, 0, 0, 1, 1, FLinearColor::White, BLEND_Translucent);
            }
        }
    }

    if (RunState != ERunState::Idle)
    {
        DrawTreadmillRun(Reporter, Height);
        return;
    }
    const double InputX = Reporter.GetMoveInput().X;
    const double Previous = LobbyX;
    LobbyX = FMath::Clamp(LobbyX + InputX * GetWorld()->GetDeltaSeconds() * 0.22, 0.05, 0.95);
    bIndoorWalking = !FMath::IsNearlyEqual(Previous, LobbyX);
    if (!FMath::IsNearlyZero(InputX))
    {
        IndoorDirection.X = InputX;
    }
    DrawReporter(Reporter, TopLeft + Size * FVector2D(LobbyX, 0.93), Height * 0.25, &IndoorDirection);

    if (bInGym)
    {
        if (FMath::Abs(LobbyX - 0.93) < 0.09)
        {
            Interaction = EInteraction::ExitGym;
            DrawInteraction(TEXT("Back to Lobby"), TopLeft + Size * FVector2D(0.93, 0.57));
        }
        else if (TreadmillBounds.bIsValid &&
            FMath::Abs(TopLeft.X + LobbyX * Size.X - TreadmillBounds.GetCenter().X) < Size.X * 0.14)
        {
            Interaction = EInteraction::StartRun;
            DrawInteraction(TEXT("Run on Treadmill"), FVector2D(TreadmillBounds.GetCenter().X, TreadmillBounds.Min.Y));
        }
    }
    else if (GymTexture && FMath::Abs(LobbyX - 0.18) < 0.11)
    {
        Interaction = EInteraction::EnterGym;
        DrawInteraction(TEXT("Enter Gym"), TopLeft + Size * FVector2D(0.18, 0.56));
    }
    else if (PoolTexture && FMath::Abs(LobbyX - 0.82) < 0.11)
    {
        Interaction = EInteraction::EnterPool;
        DrawInteraction(TEXT("Enter Pool"), TopLeft + Size * FVector2D(0.82, 0.60));
    }
    else if (FMath::Abs(LobbyX - 0.50) < 0.11)
    {
        Interaction = EInteraction::Exit;
        DrawInteraction(TEXT("Exit to Street"), TopLeft + Size * FVector2D(0.50, 0.97));
    }
}

void AReporterHUD::DrawPoolDeck(const AReporterPawn& Reporter, FVector2D TopLeft, FVector2D Size)
{
    const auto CanWalk = [](FVector2D Foot)
    {
        // The rear wall turns toward the camera at either end of the artwork.
        const double SideDepth = FMath::Clamp((FMath::Abs(Foot.X - 0.5) - 0.385) / 0.115, 0.0, 1.0);
        if (Foot.X < 0.035 || Foot.X > 0.965 || Foot.Y < 0.51 + 0.057 * SideDepth || Foot.Y > 0.97)
        {
            return false;
        }
        // The pool rim is a trapezoid in the image. Keep a small margin around it.
        if (Foot.Y < 0.537 || Foot.Y > 0.86)
        {
            return true;
        }
        const double Depth = (Foot.Y - 0.537) / (0.86 - 0.537);
        return Foot.X < FMath::Lerp(0.191, 0.087, Depth) ||
            Foot.X > FMath::Lerp(0.809, 0.910, Depth);
    };

    const FVector2D Previous = PoolPosition;
    const FVector2D Input = Reporter.GetMoveInput().GetClampedToMaxSize(1.0);
    const FVector2D Step = Input * FVector2D(0.22, 0.30) * FMath::Min(GetWorld()->GetDeltaSeconds(), 0.05f);
    if (CanWalk(PoolPosition + FVector2D(Step.X, 0)))
    {
        PoolPosition.X += Step.X;
    }
    if (CanWalk(PoolPosition + FVector2D(0, Step.Y)))
    {
        PoolPosition.Y += Step.Y;
    }
    const FVector2D Motion = PoolPosition - Previous;
    bIndoorWalking = !Motion.IsNearlyZero();
    if (bIndoorWalking)
    {
        IndoorDirection = FMath::Abs(Motion.X) > FMath::Abs(Motion.Y)
            ? FVector2D(Motion.X, 0) : FVector2D(0, Motion.Y);
    }
    DrawReporter(Reporter, TopLeft + Size * PoolPosition, Size.Y * 0.24, &IndoorDirection);

    if (PoolPosition.X < 0.14 && PoolPosition.Y < 0.65)
    {
        Interaction = EInteraction::ExitPool;
        DrawInteraction(TEXT("Back to Lobby"), TopLeft + Size * FVector2D(0.057, 0.42));
    }
    else if (PoolPosition.X < 0.22 && PoolPosition.Y >= 0.65 && PoolPosition.Y < 0.88)
    {
        Interaction = EInteraction::StartSwim;
        DrawInteraction(TEXT("Swim Race"), TopLeft + Size * FVector2D(0.14, 0.67));
    }
}

void AReporterHUD::DrawLobby(const AReporterPawn& Reporter)
{
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const double Aspect = double(LobbyTexture->GetSizeX()) / LobbyTexture->GetSizeY();
    const double Height = FMath::Min(View.Y, View.X / Aspect);
    const FVector2D Size(Height * Aspect, Height);
    const FVector2D TopLeft = (View - Size) * 0.5;
    const auto Point = [&](double X, double Y) { return TopLeft + Size * FVector2D(X, Y); };
    DrawRect(FLinearColor(0.07f, 0.045f, 0.05f), 0, 0, View.X, View.Y);
    DrawTexture(LobbyTexture, TopLeft.X, TopLeft.Y, Size.X, Size.Y,
        0, 0, 1, 1, FLinearColor::White, BLEND_Opaque);

    constexpr double LowerFloor = 0.960;
    constexpr double UpperFloor = 0.465;
    double FootY = bUpstairs ? UpperFloor : LowerFloor;
    bIndoorWalking = false;
    const double Delta = GetWorld()->GetDeltaSeconds();
    if (StairProgress >= 0.0)
    {
        StairProgress = FMath::Min(1.0, StairProgress + Delta / 1.2);
        const double Progress = bUpstairs ? 1.0 - StairProgress : StairProgress;
        LobbyX = FMath::Lerp(0.865, 0.955, 1.0 - FMath::Abs(2.0 * Progress - 1.0));
        FootY = FMath::Lerp(LowerFloor, UpperFloor, Progress);
        IndoorDirection.X = (Progress < 0.5) != bUpstairs ? 1 : -1;
        bIndoorWalking = true;
        if (StairProgress >= 1.0)
        {
            bUpstairs = !bUpstairs;
            StairProgress = -1.0;
        }
    }
    else if (!bShowGuide)
    {
        const double InputX = Reporter.GetMoveInput().X;
        const double Previous = LobbyX;
        LobbyX = FMath::Clamp(LobbyX + InputX * Delta * 0.22, 0.05, 0.92);
        bIndoorWalking = !FMath::IsNearlyEqual(Previous, LobbyX);
        if (!FMath::IsNearlyZero(InputX))
        {
            IndoorDirection.X = InputX;
        }
    }
    DrawReporter(Reporter, Point(LobbyX, FootY), Height * 0.19, &IndoorDirection);

    if (bShowGuide)
    {
        const FVector2D Box = Point(0.17, 0.62);
        DrawRect(FLinearColor(0.10f, 0.06f, 0.10f, 0.95f), Box.X, Box.Y, Size.X * 0.43, Height * 0.18);
        const float TextScale = FMath::Max(0.8f, float(Height / 580.0));
        DrawUiText(TEXT("Field Guide\nC: Camera   Arrows: Aim   Wheel: Zoom\nClick or Enter: Take a photo"),
            FLinearColor::White, Box.X + 12, Box.Y + 10, GEngine->GetSmallFont(), TextScale);
        Interaction = EInteraction::Reception;
        DrawInteraction(TEXT("Close Guide"), Point(0.22, 0.76));
        return;
    }
    if (StairProgress >= 0.0)
    {
        return;
    }
    const TCHAR* Label = nullptr;
    double MarkerX = LobbyX;
    if (LobbyX >= 0.83)
    {
        Interaction = EInteraction::Stairs;
        Label = bUpstairs ? TEXT("Go Downstairs") : TEXT("Go Upstairs");
        MarkerX = 0.90;
    }
    else if (!bUpstairs && LobbyX < 0.13)
    {
        Interaction = EInteraction::Exit;
        Label = TEXT("Exit to Street");
        MarkerX = 0.09;
    }
    else if (!bUpstairs && FMath::Abs(LobbyX - 0.22) < 0.08)
    {
        Interaction = EInteraction::Reception;
        Label = TEXT("Field Guide");
        MarkerX = 0.22;
    }
    else
    {
        for (double Door : {0.22, 0.50, 0.76})
        {
            if (FMath::Abs(LobbyX - Door) < 0.055 && (bUpstairs || Door > 0.22))
            {
                const bool bEditorial = !bUpstairs && Door == 0.50;
                Interaction = bEditorial ? EInteraction::Editorial : EInteraction::Locked;
                Label = bEditorial ? TEXT("Editorial") : TEXT("Locked Room");
                MarkerX = Door;
                break;
            }
        }
    }
    if (Label)
    {
        DrawInteraction(Label, Point(MarkerX, FootY - 0.26));
    }
    if (GetWorld()->GetRealTimeSeconds() < LobbyNoticeUntil)
    {
        const FVector2D Notice = Point(0.40, 0.53);
        DrawRect(FLinearColor(0.10f, 0.06f, 0.10f, 0.9f), Notice.X, Notice.Y, 230, 30);
        DrawUiText(LobbyNotice, FLinearColor::White, Notice.X + 10, Notice.Y + 7, GEngine->GetSmallFont());
    }
}

AReporterGameMode::AReporterGameMode()
{
    DefaultPawnClass = AReporterPawn::StaticClass();
    HUDClass = AReporterHUD::StaticClass();
}

AActor* AReporterGameMode::ChoosePlayerStart_Implementation(AController*)
{
    return GetWorld()->SpawnActor<APlayerStart>(
        FVector(0.0, CurbTop - 20.0, 0.0), FRotator::ZeroRotator);
}
