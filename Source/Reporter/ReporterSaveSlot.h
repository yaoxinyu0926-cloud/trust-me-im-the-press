#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ReporterSaveSlot.generated.h"

UCLASS()
class UReporterSaveSlot : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY()
    int32 Version = 1;

    UPROPERTY()
    FString ReporterName;

    UPROPERTY()
    double GameMinutes = 0;

    UPROPERTY()
    int32 Location = 0;

    UPROPERTY()
    uint32 Checksum = 0;

    UPROPERTY()
    TArray<uint8> GameState;
};
