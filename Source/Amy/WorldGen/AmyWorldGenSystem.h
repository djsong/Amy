// Copyright DJ Song Super-Star. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AmyWorldGenSettings.h"

class UTexture2D;
class UWorld;

void OverwriteTextureData(UTexture2D* Texture, const TArray<FColor>& InWriteColor);

/** Intermediate form for whole world generating system. 
 * The result of NNE inference and being the label for generating world.  */
struct FAmyWorldGenInferredMapInfo
{
	int32 ImageWidth = 0;
	int32 ImageHeight = 0;
	TArray<FColor> Data; // Array num is assumed as ImageWidth * ImageHeight

	bool IsValid() const
	{
		return (ImageWidth > 0) && (ImageHeight > 0) && (Data.Num() == ImageWidth * ImageHeight);
	}
};

/** Putting core functionalities for NNE based image inference and 3D world generation. */
class FAmyWorldGenSystem 
{
public:
	FAmyWorldGenSystem();

	/** As the first pass, get the inferred intermediate info to be used for world generation stage.
	 * @param OutInferredMapInfo : Dimensions will be set inside, so just send empty one here.
	 */
	static bool GetInferredMapInfo(const FAmyWorldGenNNEInferenceSettings& InferenceSettings, FAmyWorldGenInferredMapInfo& OutInferredMapInfo);

	/** Second pass, 
	 * generate actors and/or modify existing actor/component to have visually matching result from the inferred (label) map info.
	 */
	static void GenerateWorldByInferredData(UWorld* InWorld, const FAmyWorldGenActorGenSettings& ActorGenSettings, const FAmyWorldGenInferredMapInfo& InferredMapInfo);

private:
	/** 
	 * Processing for single inferred batch data. In normal circumstance, single inferred batch data spawns single actor in the world.
	 * @param BatchRefIndex : Float casted number of the first (upper left) batch data index in the label image.
	 * @param InferredMapSize : Float casted number of inferred label image data size.
	 */
	static void SpawnObjForSingleInferredBatch(UWorld* InWorld, const FAmyWorldGenActorGenSettings& ActorGenSettings, const TArray<FColor>& InferredLabelBatch, 
		const FVector2D BatchRefIndex, const FVector2D InferredMapSize);

	static EAmyWorldGenObjTypes CalcTypeFromInferredLabelBatch(const FAmyWorldGenActorGenSettings& ActorGenSettings, const TArray<FColor>& InferredLabelBatch);
};



