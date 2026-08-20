// Copyright 2026 Simulated Flow. All Rights Reserved.

using UnrealBuildTool;

public class CaptionCue : ModuleRules
{
	public CaptionCue(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// One runtime module, nothing else. Captions are an accessibility feature, which means they have to
		// work in the build the player actually gets: no UMG dependency, no UnrealEd, no third-party code.
		// SlateCore is here for FSlateFontInfo and the font measure service, RenderCore for the white texture
		// the background box and the direction arrows are drawn with.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"SlateCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
		});
	}
}
