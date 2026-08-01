#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ProtobufWriter.generated.h"

/**
 * Builds a protobuf message by setting fields by name, then serializes it.
 *
 * Blueprint flow:
 *   1. Create(ProtoRootDir, ProtoFile, MessageType, Writer, Error)
 *   2. Writer → SetString / SetInt32 / SetFloat / SetBool
 *   3. Writer → SaveToFile  — write .bin to disk
 *      Writer → SerializeToBytes — get raw bytes (e.g. for network send)
 */
UCLASS(BlueprintType)
class PROTOBUFRUNTIME_API UProtobufWriter : public UObject
{
	GENERATED_BODY()

public:
	// -----------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------

	/**
	 * Create a writer for a specific message type.
	 * The message starts empty — call Set* to populate fields.
	 *
	 * @param ProtoRootDir  Directory containing your .proto files.
	 * @param ProtoFile     Relative path to the .proto, e.g. "player_info.proto".
	 * @param MessageType   Fully-qualified message name, e.g. "game.PlayerInfo".
	 * @param OutWriter     Ready-to-use writer on success, null on failure.
	 * @param OutError      Error description on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Protobuf",
	          meta = (ExpandBoolAsExecs = "ReturnValue"))
	static bool Create(const FString& ProtoRootDir,
	                   const FString& ProtoFile,
	                   const FString& MessageType,
	                   UProtobufWriter*& OutWriter,
	                   FString& OutError);

	// -----------------------------------------------------------------------
	// Field setters — return false if the field doesn't exist or type mismatch
	// -----------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	bool SetString(const FString& FieldName, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	bool SetInt32(const FString& FieldName, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	bool SetFloat(const FString& FieldName, float Value);

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	bool SetBool(const FString& FieldName, bool Value);

	/** Reset all fields to default (empty message). */
	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	void Clear();

	// -----------------------------------------------------------------------
	// Repeated field appenders (proto3 "repeated" → Blueprint array)
	// Call multiple times to append elements one by one.
	// -----------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	bool AddString(const FString& FieldName, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	bool AddInt32(const FString& FieldName, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	bool AddFloat(const FString& FieldName, float Value);

	UFUNCTION(BlueprintCallable, Category = "Protobuf")
	bool AddBool(const FString& FieldName, bool Value);

	// -----------------------------------------------------------------------
	// Serialization
	// -----------------------------------------------------------------------

	/**
	 * Serialize the current message to a byte array.
	 * Pass the result to UProtobufReader::CreateFromBytes or send over network.
	 */
	UFUNCTION(BlueprintCallable, Category = "Protobuf",
	          meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool SerializeToBytes(TArray<uint8>& OutData, FString& OutError) const;

	/**
	 * Serialize the current message and write it to a file.
	 *
	 * @param FilePath  Absolute path to write, e.g. "C:/Save/player.bin".
	 */
	UFUNCTION(BlueprintCallable, Category = "Protobuf",
	          meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool SaveToFile(const FString& FilePath, FString& OutError) const;

private:
	struct FImpl;
	TSharedPtr<FImpl> Impl;
};
