// Copyright DJ Song Super-Star. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AmyGameMode.h"
#include "AmyWorldGenSettings.h"
#include "AmyWorldGenGameMode.generated.h"

UCLASS(abstract)
class AAmyWorldGenGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	FAmyWorldGenSettings WorldGenSettings;

	
	AAmyWorldGenGameMode();
};
