#include "ReporterGameMode.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "PaperSprite.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"

namespace
{
const FName FountainSprite(TEXT("SP_Fountain"));
const FName BoardSprite(TEXT("SP_Notice_Board"));
const FName BinSprite(TEXT("SP_Trash_Bin"));
const FLinearColor Ink(0.10f, 0.067f, 0.044f);
const FLinearColor Cream(0.96f, 0.89f, 0.74f);
const FLinearColor Teal(0.13f, 0.37f, 0.32f);
}

void AReporterHUD::UpdateStreetFinds()
{
    const int32 Day = GetGameDay();
    if (LastFindDay == Day)
    {
        return;
    }
    LastFindDay = Day;
    for (const AParkPropActor* Prop : ParkProps)
    {
        if (!IsValid(Prop) || !Prop->Artwork || Prop->Artwork->GetFName() != FountainSprite)
        {
            continue;
        }
        const FVector2D Base = Prop->GetBase();
        const double Y = FMath::Clamp(Base.Y + 62.0, 135.0, 250.0);
        if (FoundCoins.Place == EItemPlace::Away && FMath::FRand() < 0.25f)
        {
            FoundCoins = {EItemPlace::Ground, FVector2D(Base.X - FMath::FRandRange(235.0f, 375.0f), Y)};
            CoinsValue = FMath::RandRange(1, 4);
        }
        if (FoundLitter.Place == EItemPlace::Away && FMath::FRand() < 0.40f)
        {
            FoundLitter = {EItemPlace::Ground, FVector2D(Base.X + FMath::FRandRange(180.0f, 300.0f), Y)};
        }
        break;
    }
}

bool AReporterHUD::HasToyOwnerMail() const
{
    return ToyReplyDay > 0 && GetGameDay() >= ToyReplyDay;
}

bool AReporterHUD::IsNearFindProp(FVector2D Ground, FName Sprite) const
{
    return ParkProps.ContainsByPredicate([&](const AParkPropActor* Prop)
    {
        if (!IsValid(Prop) || !Prop->Artwork || Prop->Artwork->GetFName() != Sprite)
        {
            return false;
        }
        const FVector2D Delta = Prop->GetBase() - Ground;
        return FMath::Abs(Delta.X) <= 115 && FMath::Abs(Delta.Y) <= 130;
    });
}

void AReporterHUD::ToggleBackpack()
{
    if (!IsTitleOpen() && !bEndingOpen && !IsMallShopOpen() && !bDesktopOpen && !bCameraMode && !bDecorating &&
        RunState == ERunState::Idle && SwimState == ESwimState::Idle && HotPotState == EHotPotState::Idle)
    {
        bReporterPanelOpen = false;
        bBackpackOpen = !bBackpackOpen;
    }
}

void AReporterHUD::SetFindNotice(const FString& Notice, EFoundItem Item)
{
    FindNotice = Notice;
    const double Now = GetWorld()->GetRealTimeSeconds();
    FindNoticeUntil = Now + 4.0;
    HeldItem = Item;
    HeldItemUntil = Now + 0.85;
}

void AReporterHUD::DrawFoundItem(EFoundItem Item, FVector2D Center, double Height)
{
    if (Item == EFoundItem::Toy && FountainToy)
    {
        const double Width = Height * FountainToy->GetSizeX() / FountainToy->GetSizeY();
        DrawTexture(FountainToy, Center.X - Width / 2, Center.Y - Height / 2, Width, Height,
            0, 0, 1, 1, FLinearColor::White, BLEND_Translucent);
        return;
    }
    const int32 ArtIndex = static_cast<int32>(Item) - static_cast<int32>(EFoundItem::Keys);
    if (LostItemArt.IsValidIndex(ArtIndex))
    {
        if (UTexture2D* Art = LostItemArt[ArtIndex])
        {
            const double Width = Height * Art->GetSizeX() / Art->GetSizeY();
            DrawTexture(Art, Center.X - Width / 2, Center.Y - Height / 2, Width, Height,
                0, 0, 1, 1, FLinearColor::White, BLEND_Translucent);
        }
        return;
    }
    const bool bCoin = Item == EFoundItem::Coins;
    const double W = Height * (bCoin ? 1.12 : 1.38);
    const double H = Height * (bCoin ? 0.65 : 0.75);
    const auto Icon = [&](FLinearColor Color, double Inset)
    {
        const int32 Sides = bCoin ? 16 : 6;
        const double Tilt = bCoin ? 0 : -17;
        const auto Vertex = [&](int32 Index)
        {
            const double Angle = UE_TWO_PI * Index / Sides;
            return Center + FVector2D(FMath::Cos(Angle) * (W / 2 - Inset),
                FMath::Sin(Angle) * (H / 2 - Inset)).GetRotated(Tilt);
        };
        TArray<FCanvasUVTri> Triangles;
        for (int32 Side = 0; Side < Sides; ++Side)
        {
            FCanvasUVTri& Tri = Triangles.AddDefaulted_GetRef();
            Tri.V0_Pos = Center;
            Tri.V1_Pos = Vertex(Side);
            Tri.V2_Pos = Vertex(Side + 1);
            Tri.V0_UV = Tri.V1_UV = Tri.V2_UV = FVector2D(0.5, 0.5);
            Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Color;
        }
        FCanvasTriangleItem Shape(Triangles, Canvas->DefaultTexture->GetResource());
        Canvas->DrawItem(Shape);
    };
    Icon(Ink, 0);
    Icon(bCoin ? FLinearColor(0.92f, 0.58f, 0.16f) : FLinearColor(0.91f, 0.87f, 0.79f), Height * 0.09);
    DrawLine(Center.X - W * 0.23, Center.Y, Center.X + W * 0.20, Center.Y - H * 0.15,
        bCoin ? Cream : FLinearColor(0.52f, 0.48f, 0.39f), FMath::Max(1.0, Height * 0.07));
}

void AReporterHUD::DrawStreetFinds(FVector2D Center, double CameraX, double Scale)
{
    const auto Project = [&](FVector2D Ground) { return Center + (Ground - FVector2D(CameraX, 0)) * Scale; };
    if (FoundCoins.Place == EItemPlace::Ground)
    {
        DrawFoundItem(EFoundItem::Coins, Project(FoundCoins.Ground), 16 * Scale);
    }
    if (FoundLitter.Place == EItemPlace::Ground)
    {
        DrawFoundItem(EFoundItem::Litter, Project(FoundLitter.Ground), 20 * Scale);
    }
    constexpr int32 Boy = 5;
    if (bToyOwnerMailRead && !bToyReturned && PedestrianAtlases.IsValidIndex(Boy) && PedestrianAtlases[Boy])
    {
        for (const AParkPropActor* Prop : ParkProps)
        {
            if (IsValid(Prop) && Prop->Artwork && Prop->Artwork->GetFName() == BoardSprite)
            {
                UTexture2D* Atlas = PedestrianAtlases[Boy];
                const FVector2D Foot = Project(Prop->GetBase() + FVector2D(83, 22));
                const double Height = 130 * Scale;
                const double Width = Height * 288.0 / Atlas->GetSizeY();
                DrawTexture(Atlas, Foot.X - Width / 2, Foot.Y - Height, Width, Height,
                    0, 0, 288.0 / Atlas->GetSizeX(), 1, FLinearColor::White, BLEND_Translucent);
                break;
            }
        }
    }
}

void AReporterHUD::DrawBackpack(FVector2D Ground)
{
    if (bDecorating || bReporterPanelOpen)
    {
        return;
    }
    const double S = GetHudScale();
    UFont* Font = GEngine->GetMediumFont();
    const auto Rect = [&](FVector2D Pos, FVector2D Size, FLinearColor Color)
    {
        DrawRect(Color, Pos.X, Pos.Y, Size.X, Size.Y);
    };
    const auto Text = [&](const FString& Label, FVector2D Pos, FLinearColor Color, float Size = 1)
    {
        DrawUiText(Label, Color, Pos.X, Pos.Y, Font, S * Size);
    };
    const auto Button = [&](EFindAction Action, const FString& Label, FVector2D Pos, double W)
    {
        const FVector2D Size(W * S, 30 * S);
        Rect(Pos, Size, Teal);
        Text(Label, Pos + FVector2D(9, 6) * S, Cream);
        FindActions.Emplace(FBox2D(Pos, Pos + Size), Action);
    };
    const FVector2D Bottom(14 * S, Canvas->ClipY - 45 * S);
    const bool bHasToy = ToyPlace == EItemPlace::Backpack;
    const bool bHasCoins = FoundCoins.Place == EItemPlace::Backpack;
    const bool bHasLitter = FoundLitter.Place == EItemPlace::Backpack;
    const bool bAtFountain = !bInLobby && IsNearFindProp(Ground, FountainSprite);
    const bool bAtBoard = !bInLobby && IsNearFindProp(Ground, BoardSprite);
    const bool bAtBin = !bInLobby && IsNearFindProp(Ground, BinSprite);
    const bool bAtReception = bInLobby && !bInEditorial && !bInSportsCenter && !bInHotPot && !bInMall && !bInHospital &&
        !bUpstairs && StairProgress < 0 && FMath::Abs(LobbyX - 0.22) < 0.08;
    const auto NearItem = [&](const FStreetFind& Item)
    {
        return !bInLobby && Item.Place == EItemPlace::Ground && FVector2D::Distance(Item.Ground, Ground) < 75;
    };

    FVector2D ActionPos = Bottom;
    const auto Action = [&](bool bAvailable, EFindAction Kind, const TCHAR* Label)
    {
        if (bAvailable)
        {
            Button(Kind, Label, ActionPos, 235);
            ActionPos.Y -= 35 * S;
        }
    };
    Action(bAtFountain && FountainToy && ToyPlace == EItemPlace::Ground, EFindAction::PickToy, TEXT("Pick up dinosaur toy"));
    Action(NearItem(FoundCoins), EFindAction::PickCoins, TEXT("Pick up loose change"));
    Action(NearItem(FoundLitter), EFindAction::PickLitter, TEXT("Pick up litter"));
    Action(bAtFountain && bHasToy, EFindAction::PutToyBack, TEXT("Set toy back for a photo"));
    Action(bAtBoard && !bToyReturned && ToyReplyDay == 0 && (bHasToy || !LatestCapturedFountainPhoto.IsEmpty()),
        EFindAction::PostNotice, TEXT("Post lost-and-found notice"));
    Action(bAtBoard && bToyOwnerMailRead && !bToyReturned, EFindAction::ReturnToy,
        bHasToy ? TEXT("Return the toy to Jamie") : TEXT("Show Jamie the fountain"));
    Action(bAtBin && bHasLitter, EFindAction::BinLitter, TEXT("Put litter in the bin"));
    Action(bAtReception && bHasCoins, EFindAction::HandInCoins, TEXT("Hand in for the police"));
    DrawLostActions(Ground, Action);

    if (bBackpackOpen)
    {
        const FVector2D Shortcuts = GetHudShortcutOrigin();
        constexpr double BagWidth = 236;
        const FVector2D Panel(Canvas->ClipX / S - 14 - BagWidth, Shortcuts.Y + 37);
        const FVector2D Pos = Panel * S;
        const int32 FoodRows = int32(FoodStock.X > 0) + int32(FoodStock.Y > 0) + int32(FoodStock.Z > 0);
        DrawHudCard(Panel.X, Panel.Y, BagWidth, 220 + (FoodRows ? 22 + FoodRows * 36 : 0), Cream);
        Text(TEXT("Backpack"), Pos + FVector2D(13, 11) * S, Teal, 1.25f);
        Text(FString::Printf(TEXT("Morality  %d"), ReporterMorality), Pos + FVector2D(13, 39) * S, Ink);
        double Row = 65;
        const auto ItemRow = [&](bool bInBag, EFoundItem Item, const FString& Label)
        {
            if (bInBag)
            {
                DrawFoundItem(Item, Pos + FVector2D(25, Row + 9) * S, 19 * S);
                Text(Label, Pos + FVector2D(43, Row) * S, Ink);
                Row += 25;
            }
        };
        ItemRow(bHasToy, EFoundItem::Toy, TEXT("Green dinosaur toy"));
        ItemRow(bHasCoins, EFoundItem::Coins, FString::Printf(TEXT("Loose change ($%d)"), CoinsValue));
        ItemRow(bHasLitter, EFoundItem::Litter, TEXT("Litter"));
        if (!LostCases.IsEmpty() && LostCases.Last().Place == EItemPlace::Backpack)
        {
            ItemRow(true, LostCases.Last().Item, LostItemName(LostCases.Last().Item));
        }
        if (Row == 65 && !FoodRows) { Text(TEXT("Empty"), Pos + FVector2D(13, Row) * S, Ink); }
        if (bHasCoins)
        {
            Button(EFindAction::KeepCoins, TEXT("Keep the change"), Pos + FVector2D(13, 162) * S, 179);
        }
        if (bPennyAchievement)
        {
            Text(TEXT("Achievement: A Penny on the Road"), Pos + FVector2D(13, 196) * S, Teal, 0.82f);
        }
        if (FoodRows)
        {
            Text(TEXT("Food"), Pos + FVector2D(13, 220) * S, Teal);
            DrawBackpackFood(Pos + FVector2D(13, 240) * S, S);
        }
    }
    if (GetWorld()->GetRealTimeSeconds() < FindNoticeUntil)
    {
        float W, H;
        MeasureUiText(FindNotice, W, H, Font, S);
        const FVector2D Pos((Canvas->ClipX - W) / 2 - 10 * S, Canvas->ClipY * 0.22);
        Rect(Pos, FVector2D(W + 20 * S, H + 14 * S), Teal);
        Text(FindNotice, Pos + FVector2D(10, 7) * S, Cream);
    }
}

bool AReporterHUD::ClickBackpack(FVector2D Pointer)
{
    for (const auto& Entry : FindActions)
    {
        if (!Entry.Key.IsInsideOrOn(Pointer))
        {
            continue;
        }
        const EFindAction Action = Entry.Value;
        FindActions.Reset();
        if (HandleLostAction(Action)) { return true; }
        switch (Action)
        {
        case EFindAction::EatBar: EatFood(0); break;
        case EFindAction::EatSandwich: EatFood(1); break;
        case EFindAction::EatMeal: EatFood(2); break;
        case EFindAction::Bag:
            ToggleBackpack();
            break;
        case EFindAction::PickToy:
            ToyPlace = EItemPlace::Backpack;
            bFountainDiscovered = true;
            SetFindNotice(TEXT("Dinosaur toy added to backpack."), EFoundItem::Toy);
            break;
        case EFindAction::PickCoins:
            FoundCoins.Place = EItemPlace::Backpack;
            SetFindNotice(TEXT("Loose change added to backpack."), EFoundItem::Coins);
            break;
        case EFindAction::PickLitter:
            FoundLitter.Place = EItemPlace::Backpack;
            SetFindNotice(TEXT("Litter added to backpack."), EFoundItem::Litter);
            break;
        case EFindAction::PutToyBack:
            ToyPlace = EItemPlace::Ground;
            SetFindNotice(TEXT("The toy is back by the fountain."), EFoundItem::Toy);
            break;
        case EFindAction::KeepCoins:
            FoundCoins.Place = EItemPlace::Away;
            ReporterCash += CoinsValue;
            --ReporterMorality;
            SetFindNotice(FString::Printf(TEXT("Kept $%d in loose change."), CoinsValue), EFoundItem::Coins);
            break;
        case EFindAction::HandInCoins:
            FoundCoins.Place = EItemPlace::Away;
            ReporterMorality += 2;
            SetFindNotice(bPennyAchievement ? TEXT("The desk will pass it on to the police.")
                : TEXT("Achievement unlocked: A Penny on the Road."), EFoundItem::Coins);
            bPennyAchievement = true;
            break;
        case EFindAction::BinLitter:
            FoundLitter.Place = EItemPlace::Away;
            ++ReporterMorality;
            SetFindNotice(TEXT("Litter binned. Morality +1."), EFoundItem::Litter);
            break;
        case EFindAction::PostNotice:
            ToyReplyDay = GetGameDay() + FMath::RandRange(1, 2);
            SetFindNotice(TEXT("Notice posted. Check your mail in a day or two."));
            break;
        case EFindAction::ReturnToy:
        {
            const bool bHandedOver = ToyPlace == EItemPlace::Backpack;
            ToyPlace = EItemPlace::Away;
            bToyReturned = true;
            ReporterMorality += 3;
            if (LatestCapturedFountainPhoto.IsEmpty())
            {
                bFirstStoryGuide = false;
            }
            SetFindNotice(TEXT("Jamie: That's my dinosaur! Thank you!"),
                bHandedOver ? EFoundItem::Toy : EFoundItem::None);
            break;
        }
        default: break;
        }
        return true;
    }
    return false;
}
