#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ProtobufReader.generated.h"

/**
 * Wraps a parsed protobuf message. Construct with CreateFromFile or CreateFromBytes,
 * then read fields by name directly — no code generation needed.
 *
 * Blueprint flow:
 *   1. LoadFileToBytes(FilePath, Data, Error)
 *   2. CreateFromBytes(ProtoRootDir, ProtoFile, MessageType, Data, Reader, Error)
 *   3. Reader → GetString / GetInt32 / GetFloat / GetBool
 */
UCLASS(BlueprintType)
class PROTOBUFRUNTIME_API UProtobufReader : public UObject
{
	GENERATED_BODY()

public:
	// -----------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------

	/**
	 * Load a .bin file from disk and parse it in one call.
	 *
	 * @param ProtoRootDir  Directory that contains your .proto files.
	 * @param ProtoFile     Relative path to the .proto file, e.g. "player_info.proto".
	 * @param MessageType   Fully-qualified message name, e.g. "game.PlayerInfo".
	 * @param BinFilePath   Absolute path to the serialized binary file.
	 * @param OutReader     The ready-to-use reader on success, nullptr on failure.
	 * @param OutError      Error description on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Protobuf",
	          meta = (ExpandBoolAsExecs = "ReturnValue"))
	static bool CreateFromFile(const FString& ProtoRootDir,
	                           const FString& ProtoFile,
	                           const FString& MessageType,
	                           const FString& BinFilePath,
	                           UProtobufReader*& OutReader,
	                           FString& OutError);

	/**
	 * Parse an already-loaded byte array.
	 *
	 * @param ProtoRootDir  Directory that contains your .proto files.
	 * @param ProtoFile     Relative path to the .proto file, e.g. "player_info.proto".
	 * @param MessageType   Fully-qualified message name, e.g. "game.PlayerInfo".
	 * @param Data          Raw serialized protobuf bytes.
	 * @param OutReader     The ready-to-use reader on success, nullptr on failure.
	 * @param OutError      Error description on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Protobuf",
	          meta = (ExpandBoolAsExecs = "ReturnValue"))
	static bool CreateFromBytes(const FString& ProtoRootDir,
	                            const FString& ProtoFile,
	                            const FString& MessageType,
	                            const TArray<uint8>& Data,
	                            UProtobufReader*& OutReader,
	                            FString& OutError);

	// -----------------------------------------------------------------------
	// File utility
	// -----------------------------------------------------------------------

	/**
	 * Load any file from disk into a byte array.
	 * Use this to read .bin files before passing them to CreateFromBytes.
	 *
	 * @param FilePath   Absolute path to the file.
	 * @param OutData    File contents as bytes.
	 * @param OutError   Error description on failure.
	 * @return           True on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	static bool LoadFileToBytes(const FString& FilePath,
	                            TArray<uint8>& OutData,
	                            FString& OutError);

	// -----------------------------------------------------------------------
	// Scalar field accessors
	// -----------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	FString GetString(const FString& FieldName) const;

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	int32 GetInt32(const FString& FieldName) const;

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	float GetFloat(const FString& FieldName) const;

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	bool GetBool(const FString& FieldName) const;

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	bool HasField(const FString& FieldName) const;

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	TArray<FString> GetFieldNames() const;

	// -----------------------------------------------------------------------
	// Repeated field accessors (proto3 "repeated" → Blueprint array)
	// -----------------------------------------------------------------------

	/** Number of elements in a repeated field. Returns 0 if not found or not repeated. */
	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	int32 GetRepeatedCount(const FString& FieldName) const;

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	TArray<FString> GetStringArray(const FString& FieldName) const;

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	TArray<int32> GetInt32Array(const FString& FieldName) const;

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	TArray<float> GetFloatArray(const FString& FieldName) const;

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	TArray<bool> GetBoolArray(const FString& FieldName) const;

private:
	struct FImpl;
	TSharedPtr<FImpl> Impl;

	// Shared init: build schema + parse bytes. Returns false and sets OutError on failure.
	bool Init(const FString& ProtoRootDir, const FString& ProtoFile,
	          const FString& MessageType, const TArray<uint8>& Data,
	          FString& OutError);
};
