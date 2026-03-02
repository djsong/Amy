// Copyright DJ Song Super-Star. All Rights Reserved.

#include "AmyWorldGenSystem.h"
#include "Amy.h"
#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/ScopedSlowTask.h"
#include "NNE.h"
#include "NNERuntime.h"
#include "NNERuntimeCPU.h"
#include "NNEModelData.h"

static TAutoConsoleVariable<int32> CVarAmyWorldGenColorBatchAverageMode(
	TEXT("AmyWorldGen.ColorBatchAverageMode"),
	1,
	TEXT("Define the way average label color batch to get final object type.\n")
	TEXT("0 : Average colors first then get the matching type from the averaged color\n")
	TEXT("1 : Calcuate type from each color then take the most frequenct one."),
	ECVF_Default);

bool ExtractTextureData(UTexture2D* Texture, TArray<FColor>& OutExtracedData)
{
	// We assume CPU availability and no compression to read the pixel data explicitly in the CPU even in the packaged build.
	if (Texture && Texture->Availability == ETextureAvailability::CPU)
	{
		if (Texture->Availability != ETextureAvailability::CPU || Texture->CompressionSettings != TC_EditorIcon || Texture->SRGB)
		{
			ensureMsgf(false, TEXT("Texture %s has unsupported format!"), *Texture->GetName());

			UE_LOG(LogAmy, Warning, TEXT("Texture %s has unsupported format : Availability (%d), Compression (%d), sRGB %d"), 
				*Texture->GetName(), static_cast<int32>(Texture->Availability), static_cast<int32>(Texture->CompressionSettings), Texture->SRGB);
		}
		else
		{
			FSharedImageConstRef TextureCPUCopy = Texture->GetCPUCopy();
			OutExtracedData.Empty();
			OutExtracedData.AddUninitialized(TextureCPUCopy->SizeX * TextureCPUCopy->SizeY);
			FMemory::Memcpy(OutExtracedData.GetData(), TextureCPUCopy->AsBGRA8().GetData(), TextureCPUCopy->SizeX * TextureCPUCopy->SizeY * sizeof(FColor));

			return true;
		}
	}

	return false;
}

/** This one can be just a convenience util to visualize intermediate result. */
void OverwriteTextureData(UTexture2D* Texture, const TArray<FColor>& InWriteColor)
{
	if (Texture && Texture->GetPlatformData())
	{
#if WITH_EDITOR
		Texture->Modify();
#endif
		FTexture2DMipMap& MipRef = Texture->GetPlatformData()->Mips[0];
		FColor* FormatedData = reinterpret_cast<FColor*>(MipRef.BulkData.Lock(LOCK_READ_WRITE));

		if (MipRef.SizeX * MipRef.SizeY == InWriteColor.Num())
		{
			FMemory::Memcpy(FormatedData, InWriteColor.GetData(), MipRef.SizeX * MipRef.SizeY * sizeof(FColor));
		}

		MipRef.BulkData.Unlock();
		Texture->UpdateResource();
	}
}

/////////////////////////////////////////////

FAmyWorldGenSystem::FAmyWorldGenSystem()
{

}

bool FAmyWorldGenSystem::GetInferredMapInfo(const FAmyWorldGenNNEInferenceSettings& InferenceSettings, FAmyWorldGenInferredMapInfo& OutInferredMapInfo)
{
	TWeakInterfacePtr<INNERuntimeCPU> NNERuntime = UE::NNE::GetRuntime<INNERuntimeCPU>(TEXT("NNERuntimeORTCpu"));

	TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance = NNERuntime.IsValid() ? NNERuntime->CreateModelCPU(InferenceSettings.ImageToLabelNNEModel)->CreateModelInstanceCPU() : nullptr;

	/*
	  Mostly coded by Google Gemini, as I have not much knowledge on Unreal NNE. =)
	 */

	if (ModelInstance.IsValid() && InferenceSettings.ImageToLabelNNEModel && InferenceSettings.ImageInferencePatchSize > 0 && InferenceSettings.BaseMapImage)
	{
		TConstArrayView<UE::NNE::FTensorDesc> InputDescs = ModelInstance->GetInputTensorDescs();

		for (const UE::NNE::FTensorDesc& Desc : InputDescs)
		{
			UE_LOG(LogAmy, Log, TEXT("Input name required by the model: %s"), *Desc.GetName());
			UE_LOG(LogAmy, Log, TEXT("Dimensions required by the model: %d"), Desc.GetShape().Rank());

			for (int32 i = 0; i < Desc.GetShape().Rank(); ++i)
			{
				UE_LOG(LogAmy, Log, TEXT("Size of each dimension[%d] : %d"), i, Desc.GetShape().GetData()[i]);
			}
		}

		const int32 InputDimensionBatch = 1;
		const int32 InputDimensionChannel = 3;
		const int32 OutputDimensionChannel = 3;

		// The numbers here should be in step with onnx training data. Cannot be changed just here.
		UE::NNE::FTensorShape InputShape = UE::NNE::FTensorShape::Make({ 
			static_cast<uint32>(InputDimensionBatch),
			static_cast<uint32>(InputDimensionChannel),
			static_cast<uint32>(InferenceSettings.ImageInferencePatchSize),
			static_cast<uint32>(InferenceSettings.ImageInferencePatchSize) });
		ModelInstance->SetInputTensorShapes({ InputShape });

		int32 HalfPatchSize = InferenceSettings.ImageInferencePatchSize / 2;
		int32 PatchPixelNum = InferenceSettings.ImageInferencePatchSize * InferenceSettings.ImageInferencePatchSize; // 961

		TArray<FColor> MapImageFileColorData;
		if (ExtractTextureData(InferenceSettings.BaseMapImage, MapImageFileColorData))
		{
			bool bInferSucceded = true;

			const int32 ImageSizeX = InferenceSettings.BaseMapImage->GetCPUCopy()->SizeX;
			const int32 ImageSizeY = InferenceSettings.BaseMapImage->GetCPUCopy()->SizeY;

			ensure(MapImageFileColorData.Num() == ImageSizeX * ImageSizeY);
			
			OutInferredMapInfo.ImageWidth = ImageSizeX;
			OutInferredMapInfo.ImageHeight = ImageSizeY;
			OutInferredMapInfo.Data.AddUninitialized(MapImageFileColorData.Num()); // Better ensure it is allocated fully serial

			FScopedSlowTask SlowTask(ImageSizeY, FText::FromString(FString::Printf(TEXT("Read and infer image %s"), *InferenceSettings.BaseMapImage->GetName())));
			SlowTask.MakeDialog();

			for (int32 TexY = 0; TexY <= ImageSizeY; ++TexY)
			{
				SlowTask.EnterProgressFrame(1.0f);
				UE_LOG(LogAmy, Log, TEXT("[P1] Map image Inferrence processing %d / %d"), TexY, ImageSizeY);

				for (int32 TexX = 0; TexX <= ImageSizeX; ++TexX)
				{
					// Now for each texel, gather PatchSize * PatchSize data and execute inference
					// While it can be done in some "loading" time, but still very brute way.

					TArray<float> InputData;
					InputData.SetNumUninitialized(InputDimensionBatch * InputDimensionChannel * InferenceSettings.ImageInferencePatchSize * InferenceSettings.ImageInferencePatchSize);

					for (int32 InputY = -HalfPatchSize; InputY <= HalfPatchSize; ++InputY)
					{
						for (int32 InputX = -HalfPatchSize; InputX <= HalfPatchSize; ++InputX)
						{
							const int32 ReadX = TexX + InputX;
							const int32 ReadY = TexY + InputY;

							FColor PixelColor(FColor::Black);
							if (ReadX >= 0 && ReadX < ImageSizeX && ReadY >= 0 && ReadY < ImageSizeY)
							{
								// If we don't check X and Y separately, it can be false coordinate even if ReadIndex is in bound, as either X or Y can be out of texture size.
								const int32 ReadIndex = ReadY * ImageSizeX + ReadX;
								PixelColor = MapImageFileColorData.IsValidIndex(ReadIndex) ? MapImageFileColorData[ReadIndex] : FColor::Black;
							}

							int32 PatchIdx = (InputY + HalfPatchSize) * InferenceSettings.ImageInferencePatchSize + (InputX + HalfPatchSize);

							// Now, it is not like RGB array, R array then G array then B array.

							// FLinearColor PixelColorL(PixelColor); <- We don't do this, because FColor -> FLinearColor conversion considers sRGB curve.
							InputData[PatchIdx] = static_cast<float>(PixelColor.R) / 255.0f; 
							InputData[PatchIdx + PatchPixelNum] = static_cast<float>(PixelColor.G) / 255.0f;
							InputData[PatchIdx + 2 * PatchPixelNum] = static_cast<float>(PixelColor.B) / 255.0f;
						}
					}

					UE::NNE::FTensorBindingCPU InputBinding;
					InputBinding.Data = InputData.GetData();
					InputBinding.SizeInBytes = InputData.Num() * sizeof(float);

					TArray<float> OutputData;
					OutputData.SetNumZeroed(OutputDimensionChannel);
					UE::NNE::FTensorBindingCPU OutputBinding;
					OutputBinding.Data = OutputData.GetData();
					OutputBinding.SizeInBytes = OutputData.Num() * sizeof(float);

					// Finally executing the Inference
					// It could be async way, if we need to make it more smooth.
					if (ModelInstance->RunSync({ InputBinding }, { OutputBinding }) == UE::NNE::IModelInstanceRunSync::ERunSyncStatus::Ok)
					{
						const FLinearColor OutputColorF(OutputData[0], OutputData[1], OutputData[2], 1.0f);

						const int32 WriteIndex = TexY * ImageSizeX + TexX;
						if (OutInferredMapInfo.Data.IsValidIndex(WriteIndex))
						{
							// As it is just a data, better not be sRGB.
							OutInferredMapInfo.Data[WriteIndex] = OutputColorF.ToFColor(false);
						}
					}
					else
					{
						UE_LOG(LogAmy, Error, TEXT("Failed NNE inference! at PixelCoord %dx%d"), TexX, TexY);
						bInferSucceded = false;
					}
				}
			}
			return bInferSucceded;
		}
	}
	return false;
}

void FAmyWorldGenSystem::GenerateWorldByInferredData(UWorld* InWorld, const FAmyWorldGenActorGenSettings& ActorGenSettings, const FAmyWorldGenInferredMapInfo& InferredMapInfo)
{
	if (IsValid(InWorld) && InferredMapInfo.IsValid())
	{
		FScopedSlowTask SlowTask(InferredMapInfo.ImageHeight, FText::FromString(FString::Printf(TEXT("Generate world by inferred Label image"))));
		SlowTask.MakeDialog();

		const FIntVector2 BatchSize = ActorGenSettings.SafeGetLabelPixelRefBatchSize();

		for (int32 LabelY = 0; LabelY < InferredMapInfo.ImageHeight; LabelY += BatchSize.Y)
		{
			SlowTask.EnterProgressFrame(1.0f);
			UE_LOG(LogAmy, Log, TEXT("[P2] Generating World processing %d / %d"), LabelY, InferredMapInfo.ImageHeight);
			for (int32 LabelX = 0; LabelX < InferredMapInfo.ImageWidth; LabelX += BatchSize.X)
			{
				TArray<FColor> ThisBatchColors;
				for (int32 BatchY = 0; BatchY < BatchSize.Y; ++BatchY)
				{
					for (int32 BatchX = 0; BatchX < BatchSize.X; ++BatchX)
					{
						const int32 LookupX = LabelX + BatchX;
						const int32 LookupY = LabelY + BatchY;
						if (LookupX >= 0 && LookupX < InferredMapInfo.ImageWidth && LookupY >= 0 && LookupY < InferredMapInfo.ImageHeight)
						{
							const int32 LookupArrayIdx = LookupX + LookupY * InferredMapInfo.ImageWidth;
							ThisBatchColors.Add(InferredMapInfo.Data[LookupArrayIdx]);
						}
					}
				}

				// The size of ThisBatchColors might be less than BatchSize.X * BatchSize.Y, at near the edge of the image dimension.

				SpawnObjForSingleInferredBatch(InWorld, ActorGenSettings, ThisBatchColors,
					FVector2D(static_cast<double>(LabelX), static_cast<double>(LabelY)),
					FVector2D(static_cast<double>(InferredMapInfo.ImageWidth), static_cast<double>(InferredMapInfo.ImageHeight)));
			}
		}
	}
}

void FAmyWorldGenSystem::SpawnObjForSingleInferredBatch(UWorld* InWorld, const FAmyWorldGenActorGenSettings& ActorGenSettings, const TArray<FColor>& InferredLabelBatch,
	const FVector2D BatchRefIndex, const FVector2D InferredMapSize)
{
	if (IsValid(InWorld) && InferredLabelBatch.Num() > 0 && InferredMapSize.X > 0.0f && InferredMapSize.Y > 0.0f)
	{
		// 1UU = 1cm
		const FVector2D WorldSizeUU = ActorGenSettings.WorldSizeKm * 1000.0f * 100.0f;
		const FVector2D WorldHalfSizeUU = WorldSizeUU * 0.5f;
		const FVector2D UUPerPixel = WorldSizeUU / InferredMapSize;
		const FVector2D WorldCoordRefBatch = (-1.0f * WorldHalfSizeUU) + (BatchRefIndex * UUPerPixel);

		const EAmyWorldGenObjTypes FinalObjectType = CalcTypeFromInferredLabelBatch(ActorGenSettings, InferredLabelBatch);
		const FAmyWorldGenSingleResourceInfo* ResourceForType = ActorGenSettings.GetRandResourceForType(FinalObjectType);
		if (ResourceForType && IsValid(ResourceForType->ResourceObject))
		{
			FActorSpawnParameters ActorSpawnParams;
			ActorSpawnParams.ObjectFlags = RF_Transient;
			FVector ActorSpawnLoc(WorldCoordRefBatch.X, WorldCoordRefBatch.Y, 
				0.0f //<- We need to deal with its altitude some time later.
			);
			ActorSpawnLoc += ResourceForType->PlacementOffset;
			FRotator ActorSpawnRot(FRotator::ZeroRotator);

			FBoxSphereBounds ResourceBound;

			UPrimitiveComponent* SpawnedMainPrimComp = nullptr;
			if (UStaticMesh* ResObjAsSm = Cast<UStaticMesh>(ResourceForType->ResourceObject))
			{
				ResourceBound = ResObjAsSm->GetBounds();

				if (AStaticMeshActor* NewSmActor = Cast<AStaticMeshActor>(InWorld->SpawnActor(AStaticMeshActor::StaticClass(), &ActorSpawnLoc, &ActorSpawnRot, ActorSpawnParams)))
				{
					NewSmActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); // Need movable mobility for runtime modifications.
					NewSmActor->GetStaticMeshComponent()->SetStaticMesh(ResObjAsSm);
					SpawnedMainPrimComp = NewSmActor->GetStaticMeshComponent();
				}
			}
			else if (USkeletalMesh* ResObjAsSkm = Cast<USkeletalMesh>(ResourceForType->ResourceObject))
			{
				ResourceBound = ResObjAsSkm->GetBounds();

				if (ASkeletalMeshActor* NewSkmActor = Cast<ASkeletalMeshActor>(InWorld->SpawnActor(ASkeletalMeshActor::StaticClass(), &ActorSpawnLoc, &ActorSpawnRot, ActorSpawnParams)))
				{
					NewSkmActor->GetSkeletalMeshComponent()->SetSkeletalMesh(ResObjAsSkm);
					SpawnedMainPrimComp = NewSkmActor->GetSkeletalMeshComponent();
				}
			}

			if (SpawnedMainPrimComp)
			{
				FVector FinalScale = ResourceForType->PlacementScale;

				if (ResourceForType->PlacementScaleRandFrac > 0.0f)
				{
					const float RandFrac = FMath::Min(ResourceForType->PlacementScaleRandFrac, 1.0f);
					
					FinalScale = FVector(
						FMath::RandRange(FinalScale.X * (1.0f - RandFrac), FinalScale.X * (1.0f + RandFrac)),
						FMath::RandRange(FinalScale.Y * (1.0f - RandFrac), FinalScale.Y * (1.0f + RandFrac)),
						FMath::RandRange(FinalScale.Z * (1.0f - RandFrac), FinalScale.Z * (1.0f + RandFrac))
					);
				}

				if (ResourceForType->bAutoCalc2DScale)
				{
					FVector2D BatchSizeUU = UUPerPixel * FVector2D(ActorGenSettings.SafeGetLabelPixelRefBatchSize());

					FVector BoundBoxSize3D = (ResourceBound.GetBox().Max - ResourceBound.GetBox().Min);
					FVector2D BoundBoxSize2D(BoundBoxSize3D.X, BoundBoxSize3D.Y);

					FVector2D Scale2D = BatchSizeUU / BoundBoxSize2D;
					FinalScale.X = Scale2D.X;
					FinalScale.Y = Scale2D.Y;
				}
				SpawnedMainPrimComp->SetWorldScale3D(FinalScale);
			}
		}
	}
}

EAmyWorldGenObjTypes FAmyWorldGenSystem::CalcTypeFromInferredLabelBatch(const FAmyWorldGenActorGenSettings& ActorGenSettings, const TArray<FColor>& InferredLabelBatch)
{
	// There are two ways deciding type from multiple color labels
	// Either averaging the colors then get the matching type, 
	// or get the matching type for each color then take the most frequent one.

	EAmyWorldGenObjTypes RetObjectType = EAmyWorldGenObjTypes::None;

	if (CVarAmyWorldGenColorBatchAverageMode.GetValueOnAnyThread() == 0)
	{
		FLinearColor AvgColor(0.0f, 0.0f, 0.0f);
		for (const FColor LabelColor : InferredLabelBatch)
		{
			AvgColor += ConvertLabelColorToLinear(LabelColor);
		}
		AvgColor /= static_cast<float>(InferredLabelBatch.Num());

		RetObjectType = ActorGenSettings.GetObjTypeFromLabelColor(AvgColor);
	}
	else
	{
		TMap<EAmyWorldGenObjTypes, int32> ObjTypeFrequencyMap;
		for (const FColor LabelColor : InferredLabelBatch)
		{
			const EAmyWorldGenObjTypes LabelObjType = ActorGenSettings.GetObjTypeFromLabelColor(LabelColor);

			if (ObjTypeFrequencyMap.Contains(LabelObjType))
			{
				ObjTypeFrequencyMap[LabelObjType] += 1;
			}
			else
			{
				ObjTypeFrequencyMap.Add(LabelObjType, 1);
			}
		}

		int32 MostFrequentNum = 0;
		for (const auto ObjTypeFrequency : ObjTypeFrequencyMap)
		{
			if (ObjTypeFrequency.Value > MostFrequentNum)
			{
				RetObjectType = ObjTypeFrequency.Key;
				MostFrequentNum = ObjTypeFrequency.Value;
			}
		}
	}

	return RetObjectType;
}