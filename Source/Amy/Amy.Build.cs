// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Amy : ModuleRules
{
	public Amy(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"NNE"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Amy",
			"Amy/Variant_Platforming",
			"Amy/Variant_Platforming/Animation",
			"Amy/Variant_Combat",
			"Amy/Variant_Combat/AI",
			"Amy/Variant_Combat/Animation",
			"Amy/Variant_Combat/Gameplay",
			"Amy/Variant_Combat/Interfaces",
			"Amy/Variant_Combat/UI",
			"Amy/Variant_SideScrolling",
			"Amy/Variant_SideScrolling/AI",
			"Amy/Variant_SideScrolling/Gameplay",
			"Amy/Variant_SideScrolling/Interfaces",
			"Amy/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
