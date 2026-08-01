// ProtobufWriter.cpp
#include "ProtobufWriter.h"

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

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogProtobufWriter, Log, All);

// ---------------------------------------------------------------------------
// Error collector
// ---------------------------------------------------------------------------
class FWriterErrorCollector : public google::protobuf::compiler::MultiFileErrorCollector
{
public:
	FString LastError;

	virtual void RecordError(absl::string_view Filename, int Line, int Column,
	                          absl::string_view Message) override
	{
		LastError = FString::Printf(TEXT("Proto error %s:%d:%d - %s"),
			UTF8_TO_TCHAR(Filename.data()), Line, Column,
			UTF8_TO_TCHAR(Message.data()));
		UE_LOG(LogProtobufWriter, Error, TEXT("%s"), *LastError);
	}

	virtual void RecordWarning(absl::string_view Filename, int Line, int Column,
	                            absl::string_view Message) override
	{
		UE_LOG(LogProtobufWriter, Warning, TEXT("Proto warning %s:%d:%d - %s"),
			UTF8_TO_TCHAR(Filename.data()), Line, Column,
			UTF8_TO_TCHAR(Message.data()));
	}
};

// ---------------------------------------------------------------------------
// Pimpl
// ---------------------------------------------------------------------------
struct UProtobufWriter::FImpl
{
	google::protobuf::compiler::DiskSourceTree       SourceTree;
	FWriterErrorCollector                            ErrorCollector;
	TUniquePtr<google::protobuf::compiler::Importer> Importer;
	google::protobuf::DynamicMessageFactory          Factory;
	TUniquePtr<google::protobuf::Message>            Message;
	const google::protobuf::Descriptor*              Desc = nullptr;

	bool Init(const FString& ProtoRootDir, const FString& ProtoFile,
	          const FString& MessageType, FString& OutError)
	{
		SourceTree.MapPath("", TCHAR_TO_UTF8(*ProtoRootDir));

		// Map well-known types
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

		Message.Reset(Factory.GetPrototype(Desc)->New());
		Message->Clear();
		return true;
	}

	const google::protobuf::FieldDescriptor* FindField(const FString& FieldName,
	                                                    google::protobuf::FieldDescriptor::Type ExpectedType) const
	{
		if (!Desc) return nullptr;
		const auto* Field = Desc->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
		if (!Field || Field->is_repeated() || Field->type() != ExpectedType) return nullptr;
		return Field;
	}

	const google::protobuf::FieldDescriptor* FindRepeatedField(const FString& FieldName,
	                                                            google::protobuf::FieldDescriptor::Type ExpectedType) const
	{
		if (!Desc) return nullptr;
		const auto* Field = Desc->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
		if (!Field || !Field->is_repeated() || Field->type() != ExpectedType) return nullptr;
		return Field;
	}
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

bool UProtobufWriter::Create(const FString& ProtoRootDir,
                              const FString& ProtoFile,
                              const FString& MessageType,
                              UProtobufWriter*& OutWriter,
                              FString& OutError)
{
	OutWriter = nullptr;

	UProtobufWriter* Writer = NewObject<UProtobufWriter>();
	Writer->Impl = MakeShared<FImpl>();
	if (!Writer->Impl->Init(ProtoRootDir, ProtoFile, MessageType, OutError))
		return false;

	OutWriter = Writer;
	return true;
}

// ---------------------------------------------------------------------------
// Field setters
// ---------------------------------------------------------------------------

bool UProtobufWriter::SetString(const FString& FieldName, const FString& Value)
{
	if (!Impl || !Impl->Message) return false;
	const auto* Field = Impl->FindField(FieldName, google::protobuf::FieldDescriptor::TYPE_STRING);
	if (!Field) return false;
	Impl->Message->GetReflection()->SetString(Impl->Message.Get(), Field, TCHAR_TO_UTF8(*Value));
	return true;
}

bool UProtobufWriter::SetInt32(const FString& FieldName, int32 Value)
{
	if (!Impl || !Impl->Message) return false;
	const auto* Field = Impl->FindField(FieldName, google::protobuf::FieldDescriptor::TYPE_INT32);
	if (!Field) return false;
	Impl->Message->GetReflection()->SetInt32(Impl->Message.Get(), Field, Value);
	return true;
}

bool UProtobufWriter::SetFloat(const FString& FieldName, float Value)
{
	if (!Impl || !Impl->Message) return false;
	const auto* Field = Impl->FindField(FieldName, google::protobuf::FieldDescriptor::TYPE_FLOAT);
	if (!Field) return false;
	Impl->Message->GetReflection()->SetFloat(Impl->Message.Get(), Field, Value);
	return true;
}

bool UProtobufWriter::SetBool(const FString& FieldName, bool Value)
{
	if (!Impl || !Impl->Message) return false;
	const auto* Field = Impl->FindField(FieldName, google::protobuf::FieldDescriptor::TYPE_BOOL);
	if (!Field) return false;
	Impl->Message->GetReflection()->SetBool(Impl->Message.Get(), Field, Value);
	return true;
}

void UProtobufWriter::Clear()
{
	if (Impl && Impl->Message)
		Impl->Message->Clear();
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

bool UProtobufWriter::SerializeToBytes(TArray<uint8>& OutData, FString& OutError) const
{
	if (!Impl || !Impl->Message)
	{
		OutError = TEXT("Writer not initialized");
		return false;
	}

	const int32 Size = Impl->Message->ByteSizeLong();
	OutData.SetNumUninitialized(Size);

	if (!Impl->Message->SerializeToArray(OutData.GetData(), Size))
	{
		OutError = TEXT("SerializeToArray failed — required fields may be missing");
		OutData.Empty();
		return false;
	}
	return true;
}

bool UProtobufWriter::SaveToFile(const FString& FilePath, FString& OutError) const
{
	TArray<uint8> Data;
	if (!SerializeToBytes(Data, OutError))
		return false;

	if (!FFileHelper::SaveArrayToFile(Data, *FilePath))
	{
		OutError = FString::Printf(TEXT("Cannot write file: %s"), *FilePath);
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Repeated field appenders
// ---------------------------------------------------------------------------

bool UProtobufWriter::AddString(const FString& FieldName, const FString& Value)
{
	if (!Impl || !Impl->Message) return false;
	const auto* Field = Impl->FindRepeatedField(FieldName, google::protobuf::FieldDescriptor::TYPE_STRING);
	if (!Field) return false;
	Impl->Message->GetReflection()->AddString(Impl->Message.Get(), Field, TCHAR_TO_UTF8(*Value));
	return true;
}

bool UProtobufWriter::AddInt32(const FString& FieldName, int32 Value)
{
	if (!Impl || !Impl->Message) return false;
	const auto* Field = Impl->FindRepeatedField(FieldName, google::protobuf::FieldDescriptor::TYPE_INT32);
	if (!Field) return false;
	Impl->Message->GetReflection()->AddInt32(Impl->Message.Get(), Field, Value);
	return true;
}

bool UProtobufWriter::AddFloat(const FString& FieldName, float Value)
{
	if (!Impl || !Impl->Message) return false;
	const auto* Field = Impl->FindRepeatedField(FieldName, google::protobuf::FieldDescriptor::TYPE_FLOAT);
	if (!Field) return false;
	Impl->Message->GetReflection()->AddFloat(Impl->Message.Get(), Field, Value);
	return true;
}

bool UProtobufWriter::AddBool(const FString& FieldName, bool Value)
{
	if (!Impl || !Impl->Message) return false;
	const auto* Field = Impl->FindRepeatedField(FieldName, google::protobuf::FieldDescriptor::TYPE_BOOL);
	if (!Field) return false;
	Impl->Message->GetReflection()->AddBool(Impl->Message.Get(), Field, Value);
	return true;
}
