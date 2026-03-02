// Copyright DJ Song Super-Star. All Rights Reserved.

#include "AmyExec.h"
#include "Amy.h"
#include "WorldGen/AmyWorldGenSystem.h"
#include "WorldGen/AmyWorldGenGameMode.h"
#include "UObject/UObjectIterator.h"
#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "NNE.h"
#include "NNERuntime.h"
#include "NNERuntimeCPU.h"
#include "NNEModelData.h"

FAmyExec GAmyExecInst;

FAmyExec::FAmyExec()
{

}

bool FAmyExec::Exec(UWorld * Inworld, const TCHAR * Cmd, FOutputDevice & Ar)
{
	if (FParse::Command(&Cmd, TEXT("NNERuntimeTest")))
	{
		// It was very simple, first time NNE test code, still requires onnx file generated in certain way.

		FString Arg1 = FParse::Token(Cmd, false);

		TWeakInterfacePtr<INNERuntimeCPU> NNERuntime = UE::NNE::GetRuntime<INNERuntimeCPU>(TEXT("NNERuntimeORTCpu"));
		
		UNNEModelData* NNEModelData = LoadObject<UNNEModelData>(nullptr, *Arg1);

		if (NNERuntime.IsValid() && NNEModelData)
		{
			TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance = NNERuntime->CreateModelCPU(NNEModelData)->CreateModelInstanceCPU();
			if (ModelInstance.IsValid())
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

				TArray<FVector3f> InputData;
				for (int32 i = 0; i < 1; ++i)
				{
					InputData.Add(FVector3f(FMath::FRandRange(0.0f, 1.0f), FMath::FRandRange(0.0f, 1.0f), FMath::FRandRange(0.0f, 1.0f)));
				}

				// Tensor shape required by NNE
				UE::NNE::FTensorShape InputShape = UE::NNE::FTensorShape::Make({ 1, 3 });
				ModelInstance->SetInputTensorShapes({ InputShape });

				UE::NNE::FTensorBindingCPU InputBinding;
				InputBinding.Data = InputData.GetData();
				InputBinding.SizeInBytes = InputData.Num() * sizeof(FVector3f);

				TArray<float> OutputData;
				OutputData.SetNumZeroed(1); 
				UE::NNE::FTensorBindingCPU OutputBinding;
				OutputBinding.Data = OutputData.GetData();
				OutputBinding.SizeInBytes = OutputData.Num() * sizeof(float);

				// Do the Inference
				if (ModelInstance->RunSync({ InputBinding }, { OutputBinding }) == UE::NNE::IModelInstanceRunSync::ERunSyncStatus::Ok)
				{
					UE_LOG(LogAmy, Log, TEXT("Inference successful! input: %s -> output: %f"), *InputData[0].ToString(), OutputData[0]);
				}
				else
				{
					UE_LOG(LogAmy, Error, TEXT("Failed Inference!"));
				}
			}
		}
		
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("TestWorldGenImageInference")))
	{
		FString Arg1 = FParse::Token(Cmd, false);
		FString Arg2 = FParse::Token(Cmd, false);
		FString Arg3 = FParse::Token(Cmd, false);

		FAmyWorldGenNNEInferenceSettings InferenceSettings;

		InferenceSettings.ImageToLabelNNEModel = LoadObject<UNNEModelData>(nullptr, *Arg1);
		InferenceSettings.BaseMapImage = LoadObject<UTexture2D>(nullptr, *Arg2);
		InferenceSettings.ImageInferencePatchSize = 63; // Should be the same as the python script extrating onnx 

		// While BaseMapImage need to have CPU Availability, this one need GPU Availability
		UTexture2D* InferredImageFile = LoadObject<UTexture2D>(nullptr, *Arg3);
		
		FAmyWorldGenInferredMapInfo InferredInfo;
		
		if (FAmyWorldGenSystem::GetInferredMapInfo(InferenceSettings, InferredInfo))
		{
			OverwriteTextureData(InferredImageFile, InferredInfo.Data);
		}

		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("TestWorldGenAll")))
	{		
		if (AAmyWorldGenGameMode* AmyGm = Cast<AAmyWorldGenGameMode>(UGameplayStatics::GetGameMode(Inworld)))
		{
			FAmyWorldGenInferredMapInfo InferredInfo;
			if (FAmyWorldGenSystem::GetInferredMapInfo(AmyGm->WorldGenSettings.NNEInferenceSettings, InferredInfo))
			{
				FAmyWorldGenSystem::GenerateWorldByInferredData(Inworld, AmyGm->WorldGenSettings.ActorGenSettings, InferredInfo);
			}
		}

		return true;
	}


	return false;
}
