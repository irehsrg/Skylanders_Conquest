// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Skylanders_Conquest : ModuleRules
{
	public Skylanders_Conquest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"AIModule",          // AAIController::MoveTo for god pathing
			"NavigationSystem"   // runtime NavMesh over the procedural map
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
