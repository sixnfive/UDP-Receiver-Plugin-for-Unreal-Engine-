// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UDPReceiver : ModuleRules
{
	public UDPReceiver(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Sockets",
				"Networking"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore"
			}
			);
	}
}
