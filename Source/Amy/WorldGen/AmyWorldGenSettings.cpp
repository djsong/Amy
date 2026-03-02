// Copyright DJ Song Super-Star. All Rights Reserved.

#include "AmyWorldGenSettings.h"
#include "Amy.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"

static TAutoConsoleVariable<float> CVarAmyWorldGenLabelColorCompareSlack(
	TEXT("AmyWorldGen.LabelColorCompareSlack"),
	0.2f,
	TEXT("How much error tolerance allowed for deciding AmyWorldGenObjTypes from ObjTypeToLabelColor setting\n")
	TEXT("0.0 requires exact matching"),
	ECVF_Default);

FAmyWorldGenNNEInferenceSettings::FAmyWorldGenNNEInferenceSettings()
{ }

FAmyWorldGenActorGenSettings::FAmyWorldGenActorGenSettings()
{ 
	ObjTypeToLabelColor.Add(EAmyWorldGenObjTypes::Apartment, FColor::Magenta);
	ObjTypeToLabelColor.Add(EAmyWorldGenObjTypes::Building, FColor::Red);
	ObjTypeToLabelColor.Add(EAmyWorldGenObjTypes::Road, FColor::Yellow);
	ObjTypeToLabelColor.Add(EAmyWorldGenObjTypes::Water, FColor::Blue);
	ObjTypeToLabelColor.Add(EAmyWorldGenObjTypes::GreenArea, FColor::Green);
}

FLinearColor FAmyWorldGenActorGenSettings::GetLabelColorForCompare(EAmyWorldGenObjTypes InObjType) const
{
	if (const FColor* FoundColor = ObjTypeToLabelColor.Find(InObjType))
	{
		return ConvertLabelColorToLinear(*FoundColor);
	}
	return FLinearColor::Black;
}

EAmyWorldGenObjTypes FAmyWorldGenActorGenSettings::GetObjTypeFromLabelColor(FColor InLabelColor) const
{
	const FLinearColor CheckColorLinear = ConvertLabelColorToLinear(InLabelColor);
	return GetObjTypeFromLabelColor(CheckColorLinear);
}

EAmyWorldGenObjTypes FAmyWorldGenActorGenSettings::GetObjTypeFromLabelColor(FLinearColor InLabelLinearColor) const
{
	for (const auto TypeToColorIt : ObjTypeToLabelColor)
	{
		const FLinearColor SettingColorLinear = ConvertLabelColorToLinear(TypeToColorIt.Value);
		ensureMsgf(FMath::IsNearlyEqual(SettingColorLinear.A, 1.0f), TEXT("We assume not to use alpha value, so it must be certain value not affecting the result."));
		// We don't take alpha value into account, so don't let it unintentionally affects the result
		const FLinearColor CheckColorAlphaIgnore(InLabelLinearColor.R, InLabelLinearColor.G, InLabelLinearColor.B, 1.0f);
		if (SettingColorLinear.Equals(CheckColorAlphaIgnore, CVarAmyWorldGenLabelColorCompareSlack.GetValueOnAnyThread()))
		{
			return TypeToColorIt.Key;
		}
	}
	return EAmyWorldGenObjTypes::None;
}

const FAmyWorldGenSingleResourceInfo* FAmyWorldGenActorGenSettings::GetRandResourceForType(EAmyWorldGenObjTypes InObjType) const
{
	if (const FAmyWorldGenObjResources* FoundResContainer = ObjTypeToRes.Find(InObjType))
	{
		if (FoundResContainer->AllResources.Num() > 0)
		{
			// For now simple random considering all the same, but we might introduce some weight later.

			const int32 RandIndex = FMath::RandRange(0, FoundResContainer->AllResources.Num() - 1);
			return &FoundResContainer->AllResources[RandIndex];
		}
	}
	return nullptr;
}


FAmyWorldGenSettings::FAmyWorldGenSettings()
{
}
