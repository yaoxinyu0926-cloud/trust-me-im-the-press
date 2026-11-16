#include "ReporterGameMode.h"

#include "Engine/Texture2D.h"
#include "PaperSprite.h"

namespace
{
const FName FountainSprite(TEXT("SP_Fountain"));
const FName BoardSprite(TEXT("SP_Notice_Board"));
const TCHAR* OwnerNames[] = {
    TEXT("Eli"), TEXT("Nora"), TEXT("Mrs. Brooks"), TEXT("Mr. Bennett"),
    TEXT("Maya"), TEXT("Jamie"), TEXT("Lucy")
};
const TCHAR* Descriptions[] = {
    TEXT("Two brass keys on a red keyring."),
    TEXT("A brown leather wallet with a worn corner."),
    TEXT("A blue textbook with a cream label.")
};
constexpr double SearchRadius = 112.0;
}

const TCHAR* AReporterHUD::LostItemName(EFoundItem Item) const
{
    switch (Item)
    {
    case EFoundItem::Keys: return TEXT("Keys");
    case EFoundItem::Wallet: return TEXT("Wallet");
    case EFoundItem::Textbook: return TEXT("Textbook");
    default: return TEXT("Lost item");
    }
}

void AReporterHUD::UpdateLostProperty()
{
    const int32 Day = GetGameDay();
    if (Day < NextLostDay || LastLostRollDay == Day || ActiveFallCase != INDEX_NONE ||
        (!LostCases.IsEmpty() && !LostCases.Last().bReturned))
    {
        return;
    }
    LastLostRollDay = Day;
    if (FMath::FRand() >= (LostCases.IsEmpty() ? 0.70f : 0.25f))
    {
        return;
    }

    for (const AParkPropActor* Prop : ParkProps)
    {
        if (!IsValid(Prop) || !Prop->Artwork || Prop->Artwork->GetFName() != FountainSprite)
        {
            continue;
        }
        FLostCase& Loss = LostCases.AddDefaulted_GetRef();
        const int32 Type = FMath::RandRange(0, UE_ARRAY_COUNT(Descriptions) - 1);
        Loss.Item = static_cast<EFoundItem>(static_cast<int32>(EFoundItem::Keys) + Type);
        Loss.Day = Day;
        Loss.Owner = Loss.Item == EFoundItem::Textbook ? FMath::RandRange(5, 6) : FMath::RandRange(1, 4);
        Loss.Finder = Loss.Item == EFoundItem::Wallet && FMath::RandBool() ? 0 : INDEX_NONE;
        const double Side = FMath::RandBool() ? 1.0 : -1.0;
        const FVector2D Base = Prop->GetBase();
        Loss.SearchGround = FVector2D(Base.X + Side * 290, FMath::Clamp(Base.Y + 67.0, 155.0, 239.0));
        Loss.ItemGround = Loss.SearchGround + FVector2D(Side * 235, 18);
        break;
    }
}

bool AReporterHUD::HasLostOwnerMail() const
{
    return !LostCases.IsEmpty() && LostCases.Last().ReplyDay > 0 && GetGameDay() >= LostCases.Last().ReplyDay;
}

FString AReporterHUD::LostOwnerMailSubject() const
{
    return FString::Printf(TEXT("About my lost %s"), *FString(LostItemName(LostCases.Last().Item)).ToLower());
}

FString AReporterHUD::LostOwnerMailSender() const
{
    return OwnerNames[LostCases.Last().Owner];
}

FString AReporterHUD::LostOwnerMailBody() const
{
    const FLostCase& Loss = LostCases.Last();
    if (Loss.bReturned)
    {
        return TEXT("I got it back. Thank you for helping!");
    }
    const int32 Type = static_cast<int32>(Loss.Item) - static_cast<int32>(EFoundItem::Keys);
    return FString::Printf(TEXT("I saw your lost-and-found notice.\n%s\n\n"
        "I'll be by the park notice board.\nCould you bring it with you?"), Descriptions[Type]);
}

bool AReporterHUD::IsLostParticipant(int32 Character) const
{
    if (LostCases.IsEmpty() || LostCases.Last().bReturned)
    {
        return false;
    }
    const FLostCase& Loss = LostCases.Last();
    const bool bSearching = GetGameDay() == Loss.Day;
    return (Character == Loss.Owner && (bSearching || Loss.bMailRead)) ||
        (Character == Loss.Finder && bSearching && Loss.Place == EItemPlace::Ground);
}

void AReporterHUD::DrawLostProperty(FVector2D Center, double CameraX, double Scale, const FLinearColor& Tint)
{
    LostPhotoSubject = FBox2D(ForceInit);
    if (LostCases.IsEmpty() || LostCases.Last().bReturned)
    {
        return;
    }
    const FLostCase& Loss = LostCases.Last();
    const auto Project = [&](FVector2D Ground) { return Center + (Ground - FVector2D(CameraX, 0)) * Scale; };
    const auto DrawResident = [&](int32 Resident, FVector2D Ground, int32 Pose, bool bRight)
    {
        if (!PedestrianAtlases.IsValidIndex(Resident) || !PedestrianAtlases[Resident]) { return; }
        UTexture2D* Atlas = PedestrianAtlases[Resident];
        const double Height = (Resident >= 5 ? 132.0 : 178.0) * Scale;
        const double Width = Height * 288.0 / Atlas->GetSizeY();
        const double FrameU = 288.0 / Atlas->GetSizeX();
        const FVector2D Foot = Project(Ground);
        DrawTexture(Atlas, Foot.X - Width / 2, Foot.Y - Height, Width, Height,
            (Pose + (bRight ? 1 : 0)) * FrameU, 0, bRight ? -FrameU : FrameU, 1, Tint, BLEND_Translucent);
    };

    const bool bSearching = GetGameDay() == Loss.Day;
    if (Loss.bMailRead)
    {
        for (const AParkPropActor* Prop : ParkProps)
        {
            if (IsValid(Prop) && Prop->Artwork && Prop->Artwork->GetFName() == BoardSprite)
            {
                DrawResident(Loss.Owner, Prop->GetBase() + FVector2D(91, 29), 0, false);
                break;
            }
        }
    }
    else if (bSearching)
    {
        const double Time = GetWorld()->GetTimeSeconds() * 0.85;
        const double Offset = FMath::Sin(Time) * 23.0;
        const bool bPacing = FMath::Abs(FMath::Cos(Time)) > 0.45;
        constexpr int32 Steps[] = {1, 0, 2, 0};
        const int32 Pose = bPacing ? Steps[FMath::FloorToInt(Time * 5.0) % UE_ARRAY_COUNT(Steps)] : 0;
        DrawResident(Loss.Owner, Loss.SearchGround + FVector2D(Offset, 0), Pose, FMath::Cos(Time) >= 0);
    }
    if (Loss.Place == EItemPlace::Ground)
    {
        const bool bFinderPresent = bSearching && Loss.Finder != INDEX_NONE;
        if (bFinderPresent)
        {
            DrawResident(Loss.Finder, Loss.ItemGround + FVector2D(55, 0), 3, false);
        }
        const FVector2D ItemAt = Project(Loss.ItemGround + FVector2D(0, bFinderPresent ? -48 : -4));
        DrawFoundItem(Loss.Item, ItemAt, (Loss.Item == EFoundItem::Textbook ? 27 : 19) * Scale);
        LostPhotoSubject = FBox2D(ItemAt - FVector2D(12, 12) * Scale, ItemAt + FVector2D(12, 12) * Scale);
    }
}

void AReporterHUD::DrawLostActions(FVector2D Ground, TFunctionRef<void(bool, EFindAction, const TCHAR*)> Action)
{
    if (bInLobby || LostCases.IsEmpty() || LostCases.Last().bReturned)
    {
        return;
    }
    const FLostCase& Loss = LostCases.Last();
    const bool bSearching = GetGameDay() == Loss.Day;
    const bool bNearOwner = bSearching && FVector2D::DistSquared(Ground, Loss.SearchGround) < FMath::Square(SearchRadius);
    const bool bNearItem = Loss.Place == EItemPlace::Ground &&
        FVector2D::DistSquared(Ground, Loss.ItemGround) < FMath::Square(SearchRadius);
    const bool bAtBoard = IsNearFindProp(Ground, BoardSprite);
    const bool bFinderPresent = bSearching && Loss.Finder != INDEX_NONE;
    Action(bNearOwner && !Loss.bTalkedOwner, EFindAction::TalkLostOwner, TEXT("Ask what they lost"));
    Action(bNearItem && bFinderPresent && !Loss.bAskedFinder, EFindAction::AskWalletFinder, TEXT("Ask about the wallet"));
    Action(bNearItem && (!bFinderPresent || Loss.bAskedFinder), EFindAction::PickLostItem,
        bFinderPresent ? TEXT("Offer to return the wallet") : TEXT("Pick up lost property"));
    Action(bAtBoard && Loss.ReplyDay == 0 && (Loss.Place == EItemPlace::Backpack || Loss.bPhotographed),
        EFindAction::PostLostNotice, TEXT("Post a lost-property notice"));
    Action(Loss.Place == EItemPlace::Backpack && (bNearOwner || (bAtBoard && Loss.bMailRead)),
        EFindAction::ReturnLostItem, TEXT("Return the lost item"));
}

bool AReporterHUD::HandleLostAction(EFindAction Action)
{
    if (LostCases.IsEmpty() || LostCases.Last().bReturned)
    {
        return false;
    }
    FLostCase& Loss = LostCases.Last();
    const int32 Type = static_cast<int32>(Loss.Item) - static_cast<int32>(EFoundItem::Keys);
    switch (Action)
    {
    case EFindAction::TalkLostOwner:
        Loss.bTalkedOwner = true;
        SetFindNotice(FString::Printf(TEXT("%s: I lost something! %s"), OwnerNames[Loss.Owner], Descriptions[Type]));
        return true;
    case EFindAction::AskWalletFinder:
        Loss.bAskedFinder = true;
        SetFindNotice(TEXT("Eli: I just picked it up. I was taking it to the front desk."));
        return true;
    case EFindAction::PickLostItem:
        Loss.Place = EItemPlace::Backpack;
        SetFindNotice(FString::Printf(TEXT("%s added to your backpack."), LostItemName(Loss.Item)), Loss.Item);
        return true;
    case EFindAction::PostLostNotice:
        Loss.ReplyDay = GetGameDay() + FMath::RandRange(1, 2);
        SetFindNotice(TEXT("Notice posted. Check your mail in a day or two."));
        return true;
    case EFindAction::ReturnLostItem:
        Loss.bReturned = true;
        Loss.Place = EItemPlace::Away;
        ReporterMorality += 3;
        NextLostDay = GetGameDay() + 3;
        SetFindNotice(FString::Printf(TEXT("%s: You found it! Thank you.  Morality +3"), OwnerNames[Loss.Owner]), Loss.Item);
        return true;
    default:
        return false;
    }
}

void AReporterHUD::ApplyWalletTemplate()
{
    const FLostCase& Loss = LostCases[StoryWalletCase];
    if (StoryAngle == 2)
    {
        DraftHeadline = TEXT("Wallet Snatched in Rivermere Park!");
        DraftBody = TEXT("A passerby was photographed taking a wallet in the park. Is it safe to set anything down in Rivermere?");
    }
    else if (StoryAngle == 1)
    {
        DraftHeadline = TEXT("Who Picked Up the Wallet?");
        DraftBody = TEXT("A photo shows a passerby reaching for a wallet. The image alone leaves an important question unanswered.");
    }
    else if (Loss.bAskedFinder)
    {
        DraftHeadline = TEXT("A Lost Wallet and a Good Samaritan");
        DraftBody = TEXT("Eli found a wallet near the park path. He told our reporter he was taking it to the front desk so the owner could recover it.");
        if (Loss.bReturned) { DraftBody += TEXT(" The wallet has since been returned."); }
    }
    else
    {
        DraftHeadline = TEXT("Wallet Found on Park Walkway");
        DraftBody = TEXT("A wallet was found on the park walkway. A photograph shows a passerby beside it. We haven't confirmed why they picked it up.");
    }
    bCustomStory = false;
    BodyPage = 0;
    HideStoryInputs();
}

bool AReporterHUD::CanPublishWalletStory() const
{
    return StoryKind != 3 || (LostCases.IsValidIndex(StoryWalletCase) && !LostCases[StoryWalletCase].bPublished &&
        ArticlePhotos.ContainsByPredicate([&](int32 Photo)
        {
            const int32* CaseIndex = WalletPhotoCases.Find(Photo);
            return CaseIndex && *CaseIndex == StoryWalletCase;
        }));
}
