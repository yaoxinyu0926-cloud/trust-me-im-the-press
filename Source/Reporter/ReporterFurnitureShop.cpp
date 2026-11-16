#include "ReporterGameMode.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

namespace
{
struct FStoreItem
{
    const TCHAR* Name;
    int32 Price;
    const TCHAR* File;
    FIntRect Views[3];
};

// Crops are in the source images' pixels, ordered front, side, and back.
const FStoreItem Items[3][3] = {
    {
        {TEXT("Pine Frame"), 20, TEXT("bed-basic"), {{111,102,611,619}, {751,157,1420,613}, {1566,157,2050,617}}},
        {TEXT("Walnut Daybed"), 45, TEXT("bed-standard"), {{82,93,615,626}, {758,143,1413,610}, {1569,93,2080,626}}},
        {TEXT("Upholstered Bed"), 80, TEXT("bed-premium"), {{94,68,624,656}, {776,121,1399,656}, {1550,68,2069,656}}}
    },
    {
        {TEXT("Pine Writing Desk"), 25, TEXT("desk-basic"), {{55,154,669,603}, {895,154,1266,603}, {1503,154,2118,603}}},
        {TEXT("Oak-Top Desk"), 50, TEXT("desk-standard"), {{58,134,756,582}, {901,133,1270,583}, {1416,135,2114,583}}},
        {TEXT("Walnut Editor's Desk"), 90, TEXT("desk-premium"), {{70,140,768,585}, {926,140,1245,585}, {1403,140,2102,585}}}
    },
    {
        {TEXT("Compact Flat Panel"), 25, TEXT("computer-premium"), {{56,142,689,655}, {852,112,1231,648}, {1485,142,2117,648}}},
        {TEXT("Studio CRT"), 55, TEXT("computer-standard"), {{148,69,633,677}, {823,73,1349,655}, {1530,69,2030,650}}},
        {TEXT("Publishing Workstation"), 95, nullptr, {{46,122,1208,1143}, {136,57,1096,1201}, {37,123,1217,1117}}}
    }
};
const TCHAR* CatalogTabs[] = {TEXT("Beds"), TEXT("Desks"), TEXT("Computers")};
const TCHAR* StarterNames[] = {TEXT("Old Bed"), TEXT("Old Desk"), TEXT("Old Computer")};
const TCHAR* ProfessionalViews[] = {
    TEXT("computer-professional-front"), TEXT("computer-professional-side"), TEXT("computer-professional-back")
};
const FLinearColor PineInk(0.024f, 0.079f, 0.051f);
const FLinearColor Forest(0.031f, 0.226f, 0.123f);
const FLinearColor Leaf(0.095f, 0.35f, 0.202f);
const FLinearColor LineGreen(0.45f, 0.62f, 0.50f);
const FLinearColor Mint(0.84f, 0.93f, 0.85f);
const FLinearColor White(0.99f, 1.0f, 0.98f);
}

bool AReporterHUD::OwnsFurniture(EFurniture Kind, int32 Tier) const
{
    return Tier == 0 || (Tier >= 1 && Tier <= 3 && (OwnedFurniture[int32(Kind)] & (1 << (Tier - 1))) != 0);
}

bool AReporterHUD::BuyFurniture(EFurniture Kind, int32 Tier)
{
    if (Tier < 1 || Tier > 3 || OwnsFurniture(Kind, Tier)) { return false; }
    const int32 Price = Items[int32(Kind)][Tier - 1].Price;
    if (ReporterCash < Price || !GetFurnitureView(Kind, Tier, 0).Texture) { return false; }
    ReporterCash -= Price;
    OwnedFurniture[int32(Kind)] |= 1 << (Tier - 1);
    return true;
}

int32 AReporterHUD::FurnitureCost(EFurniture Kind, int32 Base) const
{
    return Base * (10 - EquippedFurniture[int32(Kind)]) / 10;
}

FString AReporterHUD::FurnitureDuration(int32 Minutes)
{
    if (Minutes % 60 == 0) { return FString::Printf(TEXT("%dh"), Minutes / 60); }
    return FString::Printf(TEXT("%dh %02dm"), Minutes / 60, Minutes % 60);
}

FString AReporterHUD::FurnitureLabel(EFurniture Kind) const
{
    const int32 Tier = EquippedFurniture[int32(Kind)];
    return Tier == 0 ? StarterNames[int32(Kind)] : Items[int32(Kind)][Tier - 1].Name;
}

void AReporterHUD::CycleFurniture(EFurniture Kind)
{
    if (Kind == EFurniture::Computer && !bHasDesk) { return; }
    bool bPlaced = Kind == EFurniture::Bed ? bHasBed : bHasDesk;
    int32& Tier = EquippedFurniture[int32(Kind)];
    if (bPlaced)
    {
        do { Tier = (Tier + 1) % 4; } while (!OwnsFurniture(Kind, Tier));
    }
    if (Kind == EFurniture::Bed)
    {
        bHasBed = bBedSelected = true;
        ConstrainBed();
    }
    else
    {
        AddDesk();
    }
    DecorStatus.Reset();
}

AReporterHUD::FFurnitureView AReporterHUD::GetFurnitureView(EFurniture Kind, int32 Tier, int32 Turn)
{
    const int32 View = Turn % 2 ? 1 : Turn == 2 ? 2 : 0;
    if (Tier > 0)
    {
        const FStoreItem& Item = Items[int32(Kind)][Tier - 1];
        const FString File = Item.File ? Item.File : ProfessionalViews[View];
        auto* Cached = FurnitureTextures.Find(File);
        if (!Cached)
        {
            const FString Path = FPaths::ProjectContentDir() / TEXT("Reporter/Furniture/ShopPreviews") / (File + TEXT(".png"));
            Cached = &FurnitureTextures.Add(File, FImageUtils::ImportFileAsTexture2D(Path));
        }
        if (UTexture2D* Texture = Cached->Get())
        {
            const FIntRect& Crop = Item.Views[View];
            const FVector2D Pixels(Texture->GetSizeX(), Texture->GetSizeY());
            return {Texture, FVector2D(Crop.Min) / Pixels, FVector2D(Crop.Size()) / Pixels,
                double(Crop.Width()) / Crop.Height()};
        }
        return {};
    }
    UTexture2D* Texture = nullptr;
    switch (Kind)
    {
    case EFurniture::Bed: Texture = BedViews.IsValidIndex(View) ? BedViews[View].Get() : nullptr; break;
    case EFurniture::Desk: Texture = DeskViews.IsValidIndex(View) ? DeskViews[View].Get() : nullptr; break;
    case EFurniture::Computer: Texture = View == 1 ? ComputerSide.Get() : ComputerFront.Get(); break;
    }
    return {Texture, FVector2D::ZeroVector, FVector2D::UnitVector,
        Texture ? double(Texture->GetSizeX()) / Texture->GetSizeY() : 1.0};
}

void AReporterHUD::DrawFurnitureView(const FFurnitureView& Art, FBox2D Bounds, bool bMirror, double Angle)
{
    if (!Art.Texture) { return; }
    DrawTexture(Art.Texture, Bounds.Min.X, Bounds.Min.Y, Bounds.GetSize().X, Bounds.GetSize().Y,
        Art.UV.X + (bMirror ? Art.UVSize.X : 0), Art.UV.Y, bMirror ? -Art.UVSize.X : Art.UVSize.X,
        Art.UVSize.Y, FLinearColor::White, BLEND_Translucent, 1.0f, false, Angle, FVector2D(0.5, 0.5));
}

void AReporterHUD::DrawMall(const AReporterPawn& Reporter)
{
    if (!MallInterior) { return; }
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const double Aspect = double(MallInterior->GetSizeX()) / MallInterior->GetSizeY();
    const double Height = FMath::Min(View.Y, View.X / Aspect);
    const FVector2D Size(Height * Aspect, Height);
    const FVector2D Origin = (View - Size) * 0.5;
    DrawRect(PineInk, 0, 0, View.X, View.Y);
    DrawTexture(MallInterior, Origin.X, Origin.Y, Size.X, Size.Y, 0, 0, 1, 1, FLinearColor::White, BLEND_Opaque);

    const FVector2D Input = IsMallShopOpen() || bReporterPanelOpen
        ? FVector2D::ZeroVector : Reporter.GetMoveInput().GetClampedToMaxSize(1.0);
    const FVector2D Old = MallPosition;
    MallPosition += Input * FVector2D(0.27, 0.25) * GetWorld()->GetDeltaSeconds();
    MallPosition.X = FMath::Clamp(MallPosition.X, 0.04, 0.96);
    MallPosition.Y = FMath::Clamp(MallPosition.Y, 0.785, 0.97);
    bIndoorWalking = !Old.Equals(MallPosition);
    if (!Input.IsNearlyZero())
    {
        IndoorDirection = Input.X != 0 ? FVector2D(Input.X, 0) : FVector2D(0, Input.Y);
    }
    DrawReporter(Reporter, Origin + Size * MallPosition, Height * 0.25, &IndoorDirection);
    if (bFurnitureShopOpen)
    {
        DrawFurnitureShop();
    }
    else if (bFoodShopOpen)
    {
        DrawFoodShop();
    }
    else if (MallPosition.X < 0.45 && MallPosition.Y < 0.90)
    {
        Interaction = EInteraction::BrowseFurniture;
        DrawInteraction(TEXT("Browse Furniture"), Origin + Size * FVector2D(0.27, 0.77));
    }
    else if (MallPosition.X > 0.58 && MallPosition.Y < 0.90)
    {
        Interaction = EInteraction::BrowseFood;
        DrawInteraction(TEXT("Buy Snacks"), Origin + Size * FVector2D(0.84, 0.77));
    }
    else if (FMath::Abs(MallPosition.X - 0.50) < 0.12 && MallPosition.Y > 0.90)
    {
        Interaction = EInteraction::Exit;
        DrawInteraction(TEXT("Exit to Street"), Origin + Size * FVector2D(0.50, 0.97));
    }
}

void AReporterHUD::DrawFurnitureShop()
{
    constexpr double Width = 1010;
    constexpr double Height = 592;
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const double S = FMath::Min(View.X / (Width + 80), View.Y / (Height + 56));
    const FVector2D Origin = (View - FVector2D(Width, Height) * S) * 0.5;
    const auto At = [&](double X, double Y) { return Origin + FVector2D(X, Y) * S; };
    const auto Rect = [&](double X, double Y, double W, double H, FLinearColor Color)
    {
        const FVector2D P = At(X, Y);
        DrawRect(Color, P.X, P.Y, W * S, H * S);
        return FBox2D(P, At(X + W, Y + H));
    };
    UFont* Regular = GEngine->GetMediumFont();
    UFont* Bold = TitleBoldFont ? TitleBoldFont.Get() : Regular;
    const auto Text = [&](const FString& Value, double X, double Y, double W, float PointSize,
        UFont* Font, FLinearColor Color, bool bCenter = true)
    {
        float Scale = PointSize * S / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size);
        float TextWidth, TextHeight;
        MeasureUiText(Value, TextWidth, TextHeight, Font, Scale);
        const float Fit = FMath::Min(1.0, W * S / FMath::Max(1.0f, TextWidth));
        const FVector2D P = At(X, Y);
        DrawUiText(Value, Color, P.X + (bCenter ? (W * S - TextWidth * Fit) * 0.5 : 0),
            P.Y, Font, Scale * Fit);
    };
    DrawRect(FLinearColor(0.014f, 0.055f, 0.036f, 0.65f), 0, 0, View.X, View.Y);
    Rect(0, 0, Width, Height, PineInk);
    Rect(3, 3, Width - 6, Height - 6, White);
    Rect(3, 3, Width - 6, 75, Forest);
    Text(TEXT("Furniture Store"), 26, 17, 625, 37, Bold, White, false);
    Rect(745, 20, 181, 38, Mint);
    Text(FString::Printf(TEXT("Cash $%d"), ReporterCash), 751, 25, 168, 21, Bold, PineInk);
    ShopClose = Rect(950, 20, 38, 38, Mint);
    Text(TEXT("X"), 950, 25, 38, 20, Bold, PineInk);

    const int32 Category = int32(FurnitureShopCategory);
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const double X = 230 + Index * 185;
        const bool bActive = Index == Category;
        ShopTabs[Index] = Rect(X, 94, 176, 40, bActive ? Forest : LineGreen);
        if (!bActive) { Rect(X + 1, 95, 174, 38, White); }
        Text(CatalogTabs[Index], X + 6, 101, 164, 20, Bold, bActive ? White : PineInk);
    }
    for (int32 Tier = 1; Tier <= 3; ++Tier)
    {
        const FStoreItem& Item = Items[Category][Tier - 1];
        const double X = 22 + (Tier - 1) * 325;
        const bool bSelected = Tier == FurnitureShopTier;
        ShopCards[Tier - 1] = Rect(X, 151, 316, 350, bSelected ? Leaf : LineGreen);
        const double Edge = bSelected ? 4 : 1;
        Rect(X + Edge, 151 + Edge, 316 - 2 * Edge, 350 - 2 * Edge, bSelected ? Mint : White);
        const FFurnitureView Art = GetFurnitureView(FurnitureShopCategory, Tier, 0);
        if (Art.Texture)
        {
            const double ArtHeight = FMath::Min(176.0, 263.0 / Art.Aspect);
            const double ArtWidth = ArtHeight * Art.Aspect;
            const double Top = 164 + (180 - ArtHeight) * 0.5;
            DrawFurnitureView(Art, FBox2D(At(X + (316 - ArtWidth) * 0.5, Top),
                At(X + (316 + ArtWidth) * 0.5, Top + ArtHeight)));
        }
        Text(Item.Name, X + 10, 354, 296, 23, Bold, PineInk);
        Text(OwnsFurniture(FurnitureShopCategory, Tier) ? FString(TEXT("Owned")) : FString::Printf(TEXT("$%d"), Item.Price),
            X + 10, 390, 296, 25, Bold, Forest);
        const int32 Base = Category == 0 ? RestMinutes : Category == 1 ? DraftStamina : DraftMinutes;
        const int32 Cost = Base * (10 - Tier) / 10;
        const FString Detail = Category == 1 ? FString::Printf(TEXT("Editing stamina: %d"), Cost)
            : FString::Printf(TEXT("%s: %s"), Category == 0 ? TEXT("Rest time") : TEXT("Editing time"),
                *FurnitureDuration(Cost));
        Text(Detail, X + 10, 430, 296, 17, Regular, PineInk);
        Text(FString::Printf(TEXT("-%d%%"), Tier * 10), X + 10, 464, 296, 17, Bold, Forest);
    }
    const int32 Price = Items[Category][FurnitureShopTier - 1].Price;
    const bool bOwned = OwnsFurniture(FurnitureShopCategory, FurnitureShopTier);
    const bool bCanBuy = !bOwned && ReporterCash >= Price;
    ShopBuy = Rect(348, 520, 314, 48, bCanBuy ? Forest : Mint);
    const FString Label = bOwned ? FString(TEXT("Available in Decorate")) : ReporterCash < Price
        ? FString(TEXT("Not enough cash")) : FString::Printf(TEXT("Buy for $%d"), Price);
    Text(Label, 362, 529, 286, 22, Bold, bCanBuy ? White : PineInk);
}

void AReporterHUD::ClickFurnitureShop(bool bUsePointer)
{
    if (!bUsePointer)
    {
        BuyFurniture(FurnitureShopCategory, FurnitureShopTier);
        return;
    }
    float X, Y;
    if (!GetOwningPlayerController()->GetMousePosition(X, Y)) { return; }
    const FVector2D Pointer(X, Y);
    if (ShopClose.bIsValid && ShopClose.IsInsideOrOn(Pointer))
    {
        bFurnitureShopOpen = false;
        return;
    }
    for (int32 Index = 0; Index < 3; ++Index)
    {
        if (ShopTabs[Index].bIsValid && ShopTabs[Index].IsInsideOrOn(Pointer))
        {
            FurnitureShopCategory = EFurniture(Index);
            FurnitureShopTier = 1;
            return;
        }
        if (ShopCards[Index].bIsValid && ShopCards[Index].IsInsideOrOn(Pointer))
        {
            FurnitureShopTier = Index + 1;
            return;
        }
    }
    if (ShopBuy.bIsValid && ShopBuy.IsInsideOrOn(Pointer))
    {
        BuyFurniture(FurnitureShopCategory, FurnitureShopTier);
    }
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReporterFurnitureTest, "Reporter.Furniture.PurchaseAndEquip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReporterFurnitureTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Furniture test world"), World)) { return false; }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    AReporterHUD* Hud = World->SpawnActor<AReporterHUD>();
    if (!TestNotNull(TEXT("Furniture test HUD"), Hud)) { return false; }
    using Kind = AReporterHUD::EFurniture;

    TestTrue(TEXT("Insufficient cash never changes ownership"), !Hud->BuyFurniture(Kind::Computer, 1) &&
        Hud->ReporterCash == 0 && !Hud->OwnsFurniture(Kind::Computer, 1));
    Hud->ReporterCash = 200;
    TestTrue(TEXT("Purchase charges once"), Hud->BuyFurniture(Kind::Computer, 3) &&
        !Hud->BuyFurniture(Kind::Computer, 3) && Hud->ReporterCash == 105);
    TestEqual(TEXT("Ownership does not apply the discount before placing"),
        Hud->FurnitureCost(Kind::Computer, 180), 180);
    Hud->bHasDesk = true;
    Hud->CycleFurniture(Kind::Computer);
    TestEqual(TEXT("Decorate skips unowned options"), Hud->EquippedFurniture.Z, 3);
    TestEqual(TEXT("Professional computer costs 126 minutes"), Hud->FurnitureCost(Kind::Computer, 180), 126);
    Hud->CycleFurniture(Kind::Computer);
    TestEqual(TEXT("Starter equipment is still selectable"), Hud->EquippedFurniture.Z, 0);

    for (int32 Category = 0; Category < 3; ++Category)
    {
        for (int32 Tier = 1; Tier <= 3; ++Tier)
        {
            for (int32 Turn = 0; Turn < 3; ++Turn)
            {
                const auto View = Hud->GetFurnitureView(Kind(Category), Tier, Turn);
                TestTrue(FString::Printf(TEXT("Art %d tier %d view %d"), Category, Tier, Turn),
                    View.Texture && View.UV.X >= 0 && View.UV.Y >= 0 &&
                    View.UV.X + View.UVSize.X <= 1 && View.UV.Y + View.UVSize.Y <= 1);
            }
        }
    }
    TestTrue(TEXT("Desk upgrade is purchasable"), Hud->BuyFurniture(Kind::Desk, 2));
    Hud->CycleFurniture(Kind::Desk);
    TestEqual(TEXT("Middle desk uses eight stamina"), Hud->FurnitureCost(Kind::Desk, 10), 8);
    TestTrue(TEXT("Bed upgrade is purchasable"), Hud->BuyFurniture(Kind::Bed, 1));
    Hud->bInEditorial = Hud->bHasBed = true;
    Hud->BedBounds = FBox2D(FVector2D::ZeroVector, FVector2D::UnitVector);
    Hud->EditorialPosition = Hud->BedPosition - FVector2D(0, Hud->GetBedRadius().Y);
    Hud->CycleFurniture(Kind::Bed);
    const double Started = Hud->OutdoorGameMinutes;
    Hud->RestAtEditorialBed();
    TestEqual(TEXT("Bed upgrade shortens short rest to 162 minutes"), Hud->OutdoorGameMinutes - Started, 162.0);
    return true;
}
#endif
