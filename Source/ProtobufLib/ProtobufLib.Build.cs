// ProtobufLib.Build.cs
// Compiles protobuf v29.5 (libprotobuf + libprotoc) and abseil-cpp from source.
// Named "ProtobufLib" to avoid conflict with UE5's built-in "Protobuf" ThirdParty (v21.1).

using System.IO;
using UnrealBuildTool;

public class ProtobufLib : ModuleRules
{
	public ProtobufLib(ReadOnlyTargetRules Target) : base(Target)
	{
		Type              = ModuleType.CPlusPlus;
		PCHUsage          = ModuleRules.PCHUsageMode.NoPCHs;
		bEnableExceptions = true;
		bUseUnity         = false;

		PublicDependencyModuleNames.Add("Core");  // needed for THIRD_PARTY_INCLUDES_START macro

		// -----------------------------------------------------------------------
		// Protobuf source paths
		// ModuleDirectory = .../Source/ProtobufLib/
		// ThirdParty      = .../Source/ThirdParty/
		// -----------------------------------------------------------------------
		string ThirdParty  = Path.Combine(ModuleDirectory, "..", "ThirdParty");
		string ProtobufSrc = Path.Combine(ThirdParty, "protobuf", "src");
		string AbslRoot    = Path.Combine(ThirdParty, "protobuf", "third_party", "abseil-cpp");
		string Utf8Root    = Path.Combine(ThirdParty, "protobuf", "third_party", "utf8_range");

		// Expose include paths publicly so dependent modules can #include protobuf headers
		PublicIncludePaths.AddRange(new string[]
		{
			ProtobufSrc,
			AbslRoot,
			Utf8Root,
		});

		// -----------------------------------------------------------------------
		// Compiler definitions
		// -----------------------------------------------------------------------
		PublicDefinitions.AddRange(new string[]
		{
			"GOOGLE_PROTOBUF_NO_RTTI=0",
			"HAVE_PTHREAD=0",
			"PROTOBUF_ENABLE_DEBUG_LOGGING_MAY_LEAK_PII=0",
			"_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING",
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("NOMINMAX");

			// DLL export/import: when building ProtobufLib.dll, export symbols;
			// dependents (ProtobufRuntime) see PROTOBUF_USE_DLLS and dllimport them.
			PublicDefinitions.Add("PROTOBUF_USE_DLLS");        // dependents get dllimport
			PublicDefinitions.Add("ABSL_CONSUME_DLL");         // absl: dependents get dllimport
			PrivateDefinitions.Add("LIBPROTOBUF_EXPORTS");     // this DLL: protobuf dllexport
			PrivateDefinitions.Add("LIBPROTOC_EXPORTS");       // this DLL: protoc dllexport
			PrivateDefinitions.Add("ABSL_BUILD_DLL");          // this DLL: absl dllexport
		}
	}
}
