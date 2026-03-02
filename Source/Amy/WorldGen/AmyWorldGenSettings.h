// Copyright DJ Song Super-Star. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AmyWorldGenSettings.generated.h"

class UNNEModelData;
class UTexture2D;
class UStaticMesh;
class USkeletalMesh;

UENUM()
enum class EAmyWorldGenObjTypes : uint8
{
	None, // Some pixels might be left unlabeled, so it can have None type.
	Apartment,
	Building,
	Road,
	Water,
	GreenArea

};

enum class EAmyWorldGenObjResTypes : uint8
{
	None, // This case is error, not like EAmyWorldGenObjTypes
	StaticMesh,
	SkeletalMesh
};

USTRUCT()
struct FAmyWorldGenSingleResourceInfo
{
	GENERATED_BODY()

	FAmyWorldGenSingleResourceInfo()
	{}

	/** Automatically fit the mesh to the area this being placed based on other settings defining world size and dimension.
	 * If true, only Z component of PlacementScale is used */
	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	bool bAutoCalc2DScale = true;

	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	FVector PlacementScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "AmyWorldGen", meta=(ClampMin=0.0, ClampMax=1.0))
	float PlacementScaleRandFrac = 0.2f;

	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	FVector PlacementOffset = FVector(0.0f, 0.0f, 0.0f);

	// Some weight for random selection might be added.
	//float RandWeight;

	UPROPERTY(EditAnywhere, Category = "AmyWorldGen", meta=(AllowedClasses = "/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh"))
	TObjectPtr<UObject> ResourceObject;
};

/** Resources that intended to be bound to one of EAmyWorldGenObjTypes */
USTRUCT()
struct FAmyWorldGenObjResources
{
	GENERATED_BODY()

	FAmyWorldGenObjResources()
	{ }

	/*
		One among them is randomly picked	
	*/
	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	TArray<FAmyWorldGenSingleResourceInfo> AllResources;
	
};

/** Settings for the first pass */
USTRUCT()
struct FAmyWorldGenNNEInferenceSettings
{
	GENERATED_BODY()

	FAmyWorldGenNNEInferenceSettings();

	/** Trained onnx data that our NNE inference will be based on, for map image to labeling. */
	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	TObjectPtr<UNNEModelData> ImageToLabelNNEModel = nullptr;

	/** How wide image area being searched for single pixel inferring. Should be matching with the size that being used while generating NNE model (onnx). */
	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	int32 ImageInferencePatchSize = 63;

	/** 3D world will be infered from this image, */
	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	TObjectPtr<UTexture2D> BaseMapImage = nullptr;


};

/** Settings for the second pass */
USTRUCT()
struct FAmyWorldGenActorGenSettings
{
	GENERATED_BODY()

	FAmyWorldGenActorGenSettings();

	/** Width and Height of area that being covered by BaseMapImage. Km, Not UU. */
	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	FVector2D WorldSizeKm = FVector2D(1.0f, 1.0f);

	/** Instead of processing Label image for each pixel basis, 
	 * we take certain number of pixels together and take average to decide which type of actor to spawn
	 * The way of taking average is defined by AmyWorldGen.ColorBatchAverageMode */
	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	FIntVector2 LabelPixelRefBatchSize = FIntVector2(4, 4);

	/** Defines which resources are used for each Object type. */
	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	TMap<EAmyWorldGenObjTypes, FAmyWorldGenObjResources> ObjTypeToRes;

	/** Defines which color each object type is being intepreted from inferred label map. */
	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	TMap<EAmyWorldGenObjTypes, FColor> ObjTypeToLabelColor;

	/** While ObjTypeToLabelColor setting specifies FColor, it uses FLinearColor for calculation. */
	FLinearColor GetLabelColorForCompare(EAmyWorldGenObjTypes InObjType) const;

	EAmyWorldGenObjTypes GetObjTypeFromLabelColor(FColor InLabelColor) const;
	EAmyWorldGenObjTypes GetObjTypeFromLabelColor(FLinearColor InLabelLinearColor) const;

	const FAmyWorldGenSingleResourceInfo* GetRandResourceForType(EAmyWorldGenObjTypes InObjType) const;

	const FIntVector2 SafeGetLabelPixelRefBatchSize() const
	{
		return FIntVector2(FMath::Max(LabelPixelRefBatchSize.X, 1), FMath::Max(LabelPixelRefBatchSize.Y, 1));
	}
};

/** FColor to FLinearColor without taking sRGB into account */
FORCEINLINE FLinearColor ConvertLabelColorToLinear(FColor InColor)
{
	// Not using FLinearColor(*FoundColor), because we don't want sRGB conversion.		
	return FLinearColor(
		static_cast<float>(InColor.R) / 255.0f,
		static_cast<float>(InColor.G) / 255.0f,
		static_cast<float>(InColor.B) / 255.0f,
		1.0f);
}

USTRUCT()
struct FAmyWorldGenSettings
{
	GENERATED_BODY()

	FAmyWorldGenSettings();

	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	FAmyWorldGenNNEInferenceSettings NNEInferenceSettings;

	UPROPERTY(EditAnywhere, Category = "AmyWorldGen")
	FAmyWorldGenActorGenSettings ActorGenSettings;
};

// Might add something like this for WorldGen global settings.
//UCLASS(MinimalAPI, config = Game, defaultconfig)
//class UAmyWorldGenGlobalSettings : public UObject