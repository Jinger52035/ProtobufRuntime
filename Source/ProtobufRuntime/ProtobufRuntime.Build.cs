// ProtobufRuntime.Build.cs
// UE wrapper module for protobuf dynamic parsing.
// All protobuf source compilation is handled by the sibling "ProtobufLib" module.

using System.IO;
using UnrealBuildTool;

public class ProtobufRuntime : ModuleRules
{
	public ProtobufRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage          = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;
		bUseUnity         = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"ProtobufLib",
		});

		// -----------------------------------------------------------------------
		// Package well-known .proto files alongside the game at runtime.
		// These are needed by DiskSourceTree when users import google/* types.
		// Destination: {OutputDir}/Plugins/ProtobufRuntime/WellKnownProtos/
		// -----------------------------------------------------------------------
		string WktSrc = Path.Combine(ModuleDirectory, "..", "ThirdParty",
		                             "protobuf", "src");

		string[] WellKnownProtos = new string[]
		{
			"google/protobuf/any.proto",
			"google/protobuf/api.proto",
			"google/protobuf/cpp_features.proto",
			"google/protobuf/descriptor.proto",
			"google/protobuf/duration.proto",
			"google/protobuf/empty.proto",
			"google/protobuf/field_mask.proto",
			"google/protobuf/source_context.proto",
			"google/protobuf/struct.proto",
			"google/protobuf/timestamp.proto",
			"google/protobuf/type.proto",
			"google/protobuf/wrappers.proto",
			"google/protobuf/compiler/plugin.proto",
		};

		foreach (string Proto in WellKnownProtos)
		{
			string SrcFile  = Path.Combine(WktSrc, Proto);
			string DestFile = Path.Combine("$(PluginDir)", "WellKnownProtos", Proto);
			RuntimeDependencies.Add(DestFile, SrcFile);
		}
	}
}
