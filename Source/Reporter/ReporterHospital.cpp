#include "ReporterGameMode.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"

bool AReporterHUD::TreatHealth()
{
    if (!bInHospital) { return false; }
    const int32 Restored = FMath::Min(TreatmentHealth, MaxHealth - ReporterHealth);
    if (Restored <= 0 || ReporterCash < TreatmentPrice)
    {
        SetFindNotice(Restored <= 0 ? TEXT("Your health is already full.") : TEXT("You need $50 for treatment."));
        return false;
    }
    ReporterCash -= TreatmentPrice;
    ReporterHealth += Restored;
    SetFindNotice(FString::Printf(TEXT("+%d health  |  $%d paid"), Restored, TreatmentPrice));
    return true;
}

void AReporterHUD::DrawHospital(const AReporterPawn& Reporter)
{
    if (!HospitalInterior) { return; }
    const FVector2D View(Canvas->ClipX, Canvas->ClipY);
    const double Aspect = double(HospitalInterior->GetSizeX()) / HospitalInterior->GetSizeY();
    const double Height = FMath::Min(View.Y, View.X / Aspect);
    const FVector2D Size(Height * Aspect, Height);
    const FVector2D Origin = (View - Size) * 0.5;
    DrawRect(FLinearColor(0.035f, 0.064f, 0.056f), 0, 0, View.X, View.Y);
    DrawTexture(HospitalInterior, Origin.X, Origin.Y, Size.X, Size.Y, 0, 0, 1, 1,
        FLinearColor::White, BLEND_Opaque);

    const double Previous = LobbyX;
    const double Input = bReporterPanelOpen ? 0.0 : Reporter.GetMoveInput().X;
    LobbyX = FMath::Clamp(LobbyX + Input * 0.28 * GetWorld()->GetDeltaSeconds(), 0.04, 0.96);
    bIndoorWalking = !FMath::IsNearlyEqual(LobbyX, Previous);
    if (Input != 0.0) { IndoorDirection = FVector2D(Input, 0); }
    DrawReporter(Reporter, Origin + Size * FVector2D(LobbyX, 0.89), Height * 0.25, &IndoorDirection);

    if (LobbyX < 0.15)
    {
        Interaction = EInteraction::Exit;
        DrawInteraction(TEXT("Exit to Street"), Origin + Size * FVector2D(0.10, 0.90));
    }
    else if (FMath::Abs(LobbyX - 0.50) < 0.16)
    {
        Interaction = EInteraction::TreatHealth;
        DrawInteraction(TEXT("Treatment $50 | +30 Health"), Origin + Size * FVector2D(0.50, 0.75));
    }
    const double S = GetHudScale();
    const double X = (View.X / S - 192) * 0.5;
    DrawHudCard(X, 20, 192, 36, FLinearColor(0.92f, 0.94f, 0.86f));
    DrawUiText(FString::Printf(TEXT("Health  %d / %d"), ReporterHealth, MaxHealth),
        FLinearColor(0.06f, 0.16f, 0.12f), (X + 16) * S, 28 * S, GEngine->GetMediumFont(), S);
}
