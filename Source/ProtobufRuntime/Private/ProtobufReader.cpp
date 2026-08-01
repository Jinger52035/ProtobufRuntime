// ProtobufReader.cpp
#include "ProtobufReader.h"

#ifdef check
#undef check
#endif
#ifdef verify
#undef verify
#endif

THIRD_PARTY_INCLUDES_START
#pragma warning(push)
#pragma warning(disable: 4100 4127 4244 4267 4312 4456 4457 4554 4701 4702 4800 4946)
#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"
#include "google/protobuf/reflection.h"
#pragma warning(pop)
THIRD_PARTY_INCLUDES_END

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogProtobufRuntime, Log, All);

// ---------------------------------------------------------------------------
// Error collector
// ---------------------------------------------------------------------------
class FProtoErrorCollector : public google::protobuf::compiler::MultiFileErrorCollector
{
public:
	FString LastError;

	virtual void RecordError(absl::string_view Filename, int Line, int Column,
	                          absl::string_view Message) override
	{
		LastError = FString::Printf(TEXT("Proto error %s:%d:%d - %s"),
			UTF8_TO_TCHAR(Filename.data()), Line, Column,
			UTF8_TO_TCHAR(Message.data()));
		UE_LOG(LogProtobufRuntime, Error, TEXT("%s"), *LastError);
	}

	virtual void RecordWarning(absl::string_view Filename, int Line, int Column,
	                            absl::string_view Message) override
	{
		UE_LOG(LogProtobufRuntime, Warning, TEXT("Proto warning %s:%d:%d - %s"),
			UTF8_TO_TCHAR(Filename.data()), Line, Column,
			UTF8_TO_TCHAR(Message.data()));
	}
};

// ---------------------------------------------------------------------------
// Pimpl
// ---------------------------------------------------------------------------
struct UProtobufReader::FImpl
{
	google::protobuf::compiler::DiskSourceTree       SourceTree;
	FProtoErrorCollector                             ErrorCollector;
	TUniquePtr<google::protobuf::compiler::Importer> Importer;
	google::protobuf::DynamicMessageFactory          Factory;
	TUniquePtr<google::protobuf::Message>            ParsedMessage;
	const google::protobuf::Descriptor*              Desc = nullptr;

	bool Init(const FString& ProtoRootDir, const FString& ProtoFile,
	          const FString& MessageType, const TArray<uint8>& Data,
	          FString& OutError)
	{
		// Map the root directory as virtual "/"
		SourceTree.MapPath("", TCHAR_TO_UTF8(*ProtoRootDir));

		// Map well-known types — needed when user protos import google/* types.
		// At runtime the WellKnownProtos dir sits next to the plugin binary.
		FString WktDir = FPaths::Combine(
			FPaths::GetPath(FModuleManager::Get().GetModuleFilename(TEXT("ProtobufRuntime"))),
			TEXT("../../WellKnownProtos"));
		FPaths::NormalizeDirectoryName(WktDir);
		SourceTree.MapPath("", TCHAR_TO_UTF8(*WktDir));
		Importer = MakeUnique<google::protobuf::compiler::Importer>(&SourceTree, &ErrorCollector);

		const google::protobuf::FileDescriptor* FileDsc = Importer->Import(TCHAR_TO_UTF8(*ProtoFile));
		if (!FileDsc)
		{
			OutError = ErrorCollector.LastError.IsEmpty()
				? FString::Printf(TEXT("Failed to import proto file: %s"), *ProtoFile)
				: ErrorCollector.LastError;
			return false;
		}

		Desc = Importer->pool()->FindMessageTypeByName(TCHAR_TO_UTF8(*MessageType));
		if (!Desc)
		{
			OutError = FString::Printf(TEXT("Message type '%s' not found in '%s'"), *MessageType, *ProtoFile);
			return false;
		}

		ParsedMessage.Reset(Factory.GetPrototype(Desc)->New());
		ParsedMessage->Clear();

		if (!ParsedMessage->ParseFromArray(Data.GetData(), Data.Num()))
		{
			OutError = FString::Printf(TEXT("Failed to parse binary data for '%s'"), *MessageType);
			return false;
		}

		return true;
	}

	const google::protobuf::FieldDescriptor* FindField(const FString& FieldName) const
	{
		return Desc ? Desc->FindFieldByName(TCHAR_TO_UTF8(*FieldName)) : nullptr;
	}

	// Finds a singular (non-repeated) field with matching type
	const google::protobuf::FieldDescriptor* FindSingularField(
		const FString& FieldName, google::protobuf::FieldDescriptor::Type Type) const
	{
		const auto* F = FindField(FieldName);
		if (!F || F->is_repeated() || F->type() != Type) return nullptr;
		return F;
	}

	// Finds a repeated field with matching type
	const google::protobuf::FieldDescriptor* FindRepeatedField(
		const FString& FieldName, google::protobuf::FieldDescriptor::Type Type) const
	{
		const auto* F = FindField(FieldName);
		if (!F || !F->is_repeated() || F->type() != Type) return nullptr;
		return F;
	}
};

// ---------------------------------------------------------------------------
// Private init helper
// ---------------------------------------------------------------------------

bool UProtobufReader::Init(const FString& ProtoRootDir, const FString& ProtoFile,
                            const FString& MessageType, const TArray<uint8>& Data,
                            FString& OutError)
{
	Impl = MakeShared<FImpl>();
	return Impl->Init(ProtoRootDir, ProtoFile, MessageType, Data, OutError);
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

bool UProtobufReader::CreateFromFile(const FString& ProtoRootDir,
                                      const FString& ProtoFile,
                                      const FString& MessageType,
                                      const FString& BinFilePath,
                                      UProtobufReader*& OutReader,
                                      FString& OutError)
{
	OutReader = nullptr;

	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *BinFilePath))
	{
		OutError = FString::Printf(TEXT("Cannot read file: %s"), *BinFilePath);
		return false;
	}

	UProtobufReader* Reader = NewObject<UProtobufReader>();
	if (!Reader->Init(ProtoRootDir, ProtoFile, MessageType, Data, OutError))
		return false;

	OutReader = Reader;
	return true;
}

bool UProtobufReader::CreateFromBytes(const FString& ProtoRootDir,
                                       const FString& ProtoFile,
                                       const FString& MessageType,
                                       const TArray<uint8>& Data,
                                       UProtobufReader*& OutReader,
                                       FString& OutError)
{
	OutReader = nullptr;

	UProtobufReader* Reader = NewObject<UProtobufReader>();
	if (!Reader->Init(ProtoRootDir, ProtoFile, MessageType, Data, OutError))
		return false;

	OutReader = Reader;
	return true;
}

// ---------------------------------------------------------------------------
// File utility
// ---------------------------------------------------------------------------

bool UProtobufReader::LoadFileToBytes(const FString& FilePath,
                                       TArray<uint8>& OutData,
                                       FString& OutError)
{
	if (!FFileHelper::LoadFileToArray(OutData, *FilePath))
	{
		OutError = FString::Printf(TEXT("Cannot read file: %s"), *FilePath);
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Field accessors
// ---------------------------------------------------------------------------

FString UProtobufReader::GetString(const FString& FieldName) const
{
	if (!Impl || !Impl->ParsedMessage) return FString();
	const auto* Field = Impl->FindSingularField(FieldName, google::protobuf::FieldDescriptor::TYPE_STRING);
	if (!Field) return FString();
	return UTF8_TO_TCHAR(Impl->ParsedMessage->GetReflection()->GetString(*Impl->ParsedMessage, Field).c_str());
}

int32 UProtobufReader::GetInt32(const FString& FieldName) const
{
	if (!Impl || !Impl->ParsedMessage) return 0;
	const auto* Field = Impl->FindSingularField(FieldName, google::protobuf::FieldDescriptor::TYPE_INT32);
	if (!Field) return 0;
	return Impl->ParsedMessage->GetReflection()->GetInt32(*Impl->ParsedMessage, Field);
}

float UProtobufReader::GetFloat(const FString& FieldName) const
{
	if (!Impl || !Impl->ParsedMessage) return 0.f;
	const auto* Field = Impl->FindSingularField(FieldName, google::protobuf::FieldDescriptor::TYPE_FLOAT);
	if (!Field) return 0.f;
	return Impl->ParsedMessage->GetReflection()->GetFloat(*Impl->ParsedMessage, Field);
}

bool UProtobufReader::GetBool(const FString& FieldName) const
{
	if (!Impl || !Impl->ParsedMessage) return false;
	const auto* Field = Impl->FindSingularField(FieldName, google::protobuf::FieldDescriptor::TYPE_BOOL);
	if (!Field) return false;
	return Impl->ParsedMessage->GetReflection()->GetBool(*Impl->ParsedMessage, Field);
}

bool UProtobufReader::HasField(const FString& FieldName) const
{
	return Impl && Impl->FindField(FieldName) != nullptr;
}

TArray<FString> UProtobufReader::GetFieldNames() const
{
	TArray<FString> Names;
	if (!Impl || !Impl->Desc) return Names;
	for (int32 i = 0; i < Impl->Desc->field_count(); ++i)
	{
		const absl::string_view Name = Impl->Desc->field(i)->name();
		Names.Add(FUTF8ToTCHAR(Name.data(), static_cast<int32>(Name.size())).Get());
	}
	return Names;
}

// ---------------------------------------------------------------------------
// Repeated field accessors
// ---------------------------------------------------------------------------

int32 UProtobufReader::GetRepeatedCount(const FString& FieldName) const
{
	if (!Impl || !Impl->ParsedMessage) return 0;
	const auto* Field = Impl->FindField(FieldName);
	if (!Field || !Field->is_repeated()) return 0;
	return Impl->ParsedMessage->GetReflection()->FieldSize(*Impl->ParsedMessage, Field);
}

TArray<FString> UProtobufReader::GetStringArray(const FString& FieldName) const
{
	TArray<FString> Out;
	if (!Impl || !Impl->ParsedMessage) return Out;
	const auto* Field = Impl->FindRepeatedField(FieldName, google::protobuf::FieldDescriptor::TYPE_STRING);
	if (!Field) return Out;
	const auto* Refl = Impl->ParsedMessage->GetReflection();
	const int32 Count = Refl->FieldSize(*Impl->ParsedMessage, Field);
	Out.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
		Out.Add(UTF8_TO_TCHAR(Refl->GetRepeatedString(*Impl->ParsedMessage, Field, i).c_str()));
	return Out;
}

TArray<int32> UProtobufReader::GetInt32Array(const FString& FieldName) const
{
	TArray<int32> Out;
	if (!Impl || !Impl->ParsedMessage) return Out;
	const auto* Field = Impl->FindRepeatedField(FieldName, google::protobuf::FieldDescriptor::TYPE_INT32);
	if (!Field) return Out;
	const auto* Refl = Impl->ParsedMessage->GetReflection();
	const int32 Count = Refl->FieldSize(*Impl->ParsedMessage, Field);
	Out.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
		Out.Add(Refl->GetRepeatedInt32(*Impl->ParsedMessage, Field, i));
	return Out;
}

TArray<float> UProtobufReader::GetFloatArray(const FString& FieldName) const
{
	TArray<float> Out;
	if (!Impl || !Impl->ParsedMessage) return Out;
	const auto* Field = Impl->FindRepeatedField(FieldName, google::protobuf::FieldDescriptor::TYPE_FLOAT);
	if (!Field) return Out;
	const auto* Refl = Impl->ParsedMessage->GetReflection();
	const int32 Count = Refl->FieldSize(*Impl->ParsedMessage, Field);
	Out.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
		Out.Add(Refl->GetRepeatedFloat(*Impl->ParsedMessage, Field, i));
	return Out;
}

TArray<bool> UProtobufReader::GetBoolArray(const FString& FieldName) const
{
	TArray<bool> Out;
	if (!Impl || !Impl->ParsedMessage) return Out;
	const auto* Field = Impl->FindRepeatedField(FieldName, google::protobuf::FieldDescriptor::TYPE_BOOL);
	if (!Field) return Out;
	const auto* Refl = Impl->ParsedMessage->GetReflection();
	const int32 Count = Refl->FieldSize(*Impl->ParsedMessage, Field);
	Out.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
		Out.Add(Refl->GetRepeatedBool(*Impl->ParsedMessage, Field, i));
	return Out;
}
