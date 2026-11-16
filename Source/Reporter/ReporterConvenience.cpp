#include "ReporterGameMode.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"

namespace
{
struct FFood
{
    const TCHAR* Name;
    int32 Price;
    int32 Stamina;
};
constexpr FFood Foods[] = {
    {TEXT("Granola Bar"), 5, 10},
    {TEXT("Deli Sandwich"), 10, 25},
    {TEXT("Chicken & Rice"), 20, 50}
};
const FLinearColor Ink(0.038f, 0.11f, 0.083f);
const FLinearColor Green(0.031f, 0.226f, 0.123f);
const FLinearColor Mint(0.84f, 0.93f, 0.85f);
const FLinearColor White(0.99f, 1.0f, 0.98f);
}

bool AReporterHUD::BuyFood(int32 Index, int32 Quantity)
{
    if (Index < 0 || Index >= UE_ARRAY_COUNT(Foods) || Quantity < 1 ||
        Quantity > MaxFoodStock - FoodStock[Index])
    {
        return false;
    }
    const int32 Cost = Foods[Index].Price * Quantity;
    if (ReporterCash < Cost) { return false; }
    ReporterCash -= Cost;
    FoodStock[Index] += Quantity;
    return true;
}

bool AReporterHUD::EatFood(int32 Index)
{
    if (Index < 0 || Index >= UE_ARRAY_COUNT(Foods) || FoodStock[Index] <= 0) { return false; }
    const int32 Restored = FMath::Clamp(100 - ReporterStamina, 0, Foods[Index].Stamina);
    if (Restored == 0)
    {
        SetFindNotice(TEXT("Your stamina is already full."));
        return false;
    }
    --FoodStock[Index];
    ReporterStamina += Restored;
    SetFindNotice(FString::Printf(TEXT("%s  |  +%d stamina"), Foods[Index].Name, Restored));
    return true;
}

void AReporterHUD::OpenFoodShop()
{
    bFoodShopOpen = true;
    bBackpackOpen = false;
    SelectedFood = 0;
    FoodQuantity = 1;
    FoodMinus = FoodPlus = FoodBuy = FoodClose = FBox2D(ForceInit);
    for (FBox2D& Card : FoodCards) { Card = FBox2D(ForceInit); }
}

void AReporterHUD::DrawFoodIcon(int32 Index, FVector2D Center, FVector2D MaxSize)
{
    if (!FoodAtlas) { return; }
    const double Side = FMath::Min(MaxSize.X, MaxSize.Y);
    DrawTexture(FoodAtlas, Center.X - Side * 0.5, Center.Y - Side * 0.5, Side, Side,
        float(Index) / 3.0f, 0, 1.0f / 3.0f, 1, FLinearColor::White, BLEND_Translucent);
}

void AReporterHUD::DrawFoodShop()
{
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const FVector2D Panel(960, 510);
    const double Scale = FMath::Min(View.X / 1020.0, View.Y / 566.0);
    const FVector2D Origin = (View - Panel * Scale) * 0.5;
    UFont* Regular = GEngine->GetMediumFont();
    UFont* Bold = TitleBoldFont ? TitleBoldFont.Get() : Regular;
    const auto At = [&](double X, double Y) { return Origin + FVector2D(X, Y) * Scale; };
    const auto Rect = [&](double X, double Y, double W, double H, FLinearColor Color)
    {
        const FVector2D P = At(X, Y);
        DrawRect(Color, P.X, P.Y, W * Scale, H * Scale);
        return FBox2D(P, At(X + W, Y + H));
    };
    const auto Text = [&](const FString& Label, double X, double Y, double W, float Size, UFont* Font, FLinearColor Color)
    {
        float Factor = Size * Scale / FMath::Max(1.0f, Font->GetLegacySlateFontInfo().Size);
        float TextWidth, TextHeight;
        MeasureUiText(Label, TextWidth, TextHeight, Font, Factor);
        const double Fit = FMath::Min(1.0, W * Scale / FMath::Max(1.0f, TextWidth));
        const FVector2D P = At(X + (W - TextWidth * Fit / Scale) * 0.5, Y);
        DrawUiText(Label, Color, P.X, P.Y, Font, Factor * Fit);
    };
    const auto Button = [&](const FString& Label, double X, double Y, double W, bool bEnabled)
    {
        const FBox2D Bounds = Rect(X, Y, W, 40, bEnabled ? Green : Mint);
        Text(Label, X + 6, Y + 7, W - 12, 19, Bold, bEnabled ? White : Ink);
        return bEnabled ? Bounds : FBox2D(ForceInit);
    };

    DrawRect(FLinearColor(0.014f, 0.055f, 0.036f, 0.65f), 0, 0, View.X, View.Y);
    Rect(0, 0, Panel.X, Panel.Y, Ink);
    Rect(3, 3, Panel.X - 6, Panel.Y - 6, White);
    Rect(3, 3, Panel.X - 6, 70, Green);
    DrawUiText(TEXT("Convenience Store"), White, At(25, 16).X, At(25, 16).Y, Bold,
        33 * Scale / FMath::Max(1.0f, Bold->GetLegacySlateFontInfo().Size));
    Rect(683, 19, 176, 37, Mint);
    Text(FString::Printf(TEXT("Cash $%d"), ReporterCash), 687, 24, 168, 20, Bold, Ink);
    FoodClose = Button(TEXT("X"), 891, 17, 44, true);

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Foods); ++Index)
    {
        const double X = 22 + 308 * Index;
        const bool bSelected = Index == SelectedFood;
        FoodCards[Index] = Rect(X, 93, 300, 288, bSelected ? Green : Mint);
        const double Border = bSelected ? 4 : 2;
        Rect(X + Border, 93 + Border, 300 - 2 * Border, 288 - 2 * Border, bSelected ? Mint : White);
        DrawFoodIcon(Index, At(X + 150, 185), FVector2D(155, 155) * Scale);
        Text(Foods[Index].Name, X + 12, 269, 276, 23, Bold, Ink);
        Text(FString::Printf(TEXT("$%d  |  +%d Stamina"), Foods[Index].Price, Foods[Index].Stamina),
            X + 12, 310, 276, 18, Regular, Green);
        Text(FString::Printf(TEXT("In backpack: %d"), FoodStock[Index]), X + 12, 345, 276, 15, Regular, Ink);
    }

    const FFood& Item = Foods[SelectedFood];
    const int32 Space = MaxFoodStock - FoodStock[SelectedFood];
    const int32 Affordable = ReporterCash / Item.Price;
    const bool bCanBuy = FoodQuantity <= Space && FoodQuantity <= Affordable;
    FoodMinus = Button(TEXT("-"), 190, 405, 48, FoodQuantity > 1);
    Text(FString::Printf(TEXT("Qty %d"), FoodQuantity), 247, 413, 86, 19, Bold, Ink);
    FoodPlus = Button(TEXT("+"), 342, 405, 48, FoodQuantity < FMath::Min(Space, Affordable));
    const FString BuyLabel = Space == 0 ? FString(TEXT("Backpack stack full")) : bCanBuy
        ? FString::Printf(TEXT("Buy for $%d"), Item.Price * FoodQuantity) : FString(TEXT("Not enough cash"));
    FoodBuy = Button(BuyLabel, 450, 405, 318, bCanBuy);
    Text(TEXT("Food goes into your backpack."), 225, 466, 510, 16, Regular, Ink);
}

void AReporterHUD::ClickFoodShop(bool bUsePointer)
{
    const auto Purchase = [&]()
    {
        if (BuyFood(SelectedFood, FoodQuantity)) { FoodQuantity = 1; }
    };
    if (!bUsePointer)
    {
        Purchase();
        return;
    }
    float X, Y;
    if (!GetOwningPlayerController()->GetMousePosition(X, Y)) { return; }
    const FVector2D Pointer(X, Y);
    const auto Hit = [&](const FBox2D& Box) { return Box.bIsValid && Box.IsInsideOrOn(Pointer); };
    if (Hit(FoodClose)) { bFoodShopOpen = false; return; }
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Foods); ++Index)
    {
        if (!Hit(FoodCards[Index])) { continue; }
        if (SelectedFood != Index)
        {
            SelectedFood = Index;
            FoodQuantity = 1;
            FoodMinus = FoodPlus = FoodBuy = FBox2D(ForceInit);
        }
        return;
    }
    if (Hit(FoodMinus)) { FoodQuantity = FMath::Max(1, FoodQuantity - 1); }
    else if (Hit(FoodPlus))
    {
        const int32 Limit = FMath::Min(MaxFoodStock - FoodStock[SelectedFood], ReporterCash / Foods[SelectedFood].Price);
        if (FoodQuantity < Limit) { ++FoodQuantity; }
    }
    else if (Hit(FoodBuy)) { Purchase(); }
}

void AReporterHUD::DrawBackpackFood(FVector2D Position, double Scale)
{
    UFont* Font = GEngine->GetMediumFont();
    constexpr EFindAction EatActions[] = {EFindAction::EatBar, EFindAction::EatSandwich, EFindAction::EatMeal};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Foods); ++Index)
    {
        if (FoodStock[Index] == 0) { continue; }
        DrawFoodIcon(Index, Position + FVector2D(14, 16) * Scale, FVector2D(29, 29) * Scale);
        DrawUiText(Foods[Index].Name, Ink, Position.X + 34 * Scale, Position.Y, Font, 0.83f * Scale);
        DrawUiText(FString::Printf(TEXT("x%d  |  +%d stamina"), FoodStock[Index], Foods[Index].Stamina),
            Ink, Position.X + 34 * Scale, Position.Y + 16 * Scale, Font, 0.73f * Scale);
        const FVector2D Button = Position + FVector2D(161, 3) * Scale;
        DrawRect(Green, Button.X, Button.Y, 49 * Scale, 26 * Scale);
        DrawUiText(TEXT("Eat"), White, Button.X + 11 * Scale, Button.Y + 4 * Scale, Font, 0.83f * Scale);
        FindActions.Emplace(FBox2D(Button, Button + FVector2D(49, 26) * Scale), EatActions[Index]);
        Position.Y += 36 * Scale;
    }
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReporterFoodTest, "Reporter.Food.PurchaseAndEat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReporterFoodTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Food test world"), World)) { return false; }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    AReporterHUD* Hud = World->SpawnActor<AReporterHUD>();
    if (!TestNotNull(TEXT("Food test HUD"), Hud)) { return false; }
    Hud->ReporterCash = 45;
    Hud->ReporterStamina = 20;
    TestTrue(TEXT("Purchase adds the requested quantities without eating"), Hud->BuyFood(0, 1) &&
        Hud->BuyFood(1, 2) && Hud->BuyFood(2, 1) && Hud->FoodStock == FIntVector(1, 2, 1) &&
        Hud->ReporterCash == 0 && Hud->ReporterStamina == 20);
    TestTrue(TEXT("Unaffordable purchase changes nothing"), !Hud->BuyFood(0, 1) &&
        Hud->FoodStock == FIntVector(1, 2, 1) && Hud->ReporterCash == 0);
    TestTrue(TEXT("Eating spends one portion and only restores stamina"), Hud->EatFood(0) &&
        Hud->EatFood(1) && Hud->ReporterStamina == 55 && Hud->FoodStock == FIntVector(0, 1, 1));
    TestTrue(TEXT("Eating caps at 100 without consuming another portion when full"), Hud->EatFood(2) &&
        Hud->ReporterStamina == 100 && !Hud->EatFood(1) && Hud->FoodStock == FIntVector(0, 1, 0));
    Hud->ReporterCash = 1000;
    TestTrue(TEXT("Capacity is enforced per food"), !Hud->BuyFood(0, 100) && Hud->BuyFood(0, 99) &&
        !Hud->BuyFood(0, 1) && Hud->FoodStock.X == 99);
    return true;
}
#endif
