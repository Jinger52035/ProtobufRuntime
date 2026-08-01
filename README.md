# ProtobufRuntime

UE5 plugin that lets you load and read `.proto` files at runtime — no code generation, no precompiled binaries. Protobuf 37.0.0-dev is compiled directly from source into the plugin.

> **Platform support**: Only tested on **UE 5.6 / Windows x64 / MSVC**. Other platforms and engine versions have not been validated and may require additional porting work (warning suppression, export macros, file path handling).

![Blueprint nodes overview](Img/Img.jpg)

---

## What it does

- Load any `.proto` schema from disk at runtime
- Parse binary protobuf data via reflection (`DynamicMessage`)
- Read fields by name from Blueprint or C++
- No `protoc` step, no generated `.pb.h/.pb.cc` files needed

---

## Plugin structure

```
ProtobufRuntime/
├── Source/
│   ├── ProtobufLib/              # Compiles protobuf + abseil from source
│   │   ├── ProtobufLib.Build.cs
│   │   └── Private/
│   │       ├── ProtobufModule.cpp
│   │       └── protobuf_src/     # 222 wrapper .cpp files (1-per-.cc)
│   ├── ProtobufRuntime/          # UE module — exposes UProtobufReader
│   │   ├── ProtobufRuntime.Build.cs
│   │   ├── Public/
│   │   │   └── ProtobufReader.h
│   │   └── Private/
│   │       └── ProtobufReader.cpp
│   └── ThirdParty/
│       └── protobuf/             # protobuf 37.0.0-dev source (src/ + third_party/)
└── ProtobufRuntime.uplugin
```

`ProtobufLib` is a pure C++ module that owns all third-party compilation. `ProtobufRuntime` only contains the UE-facing API and depends on `ProtobufLib`.

---

## Setup

Clone protobuf with submodules into the ThirdParty directory. The bundled source currently identifies itself as Protobuf 37.0.0-dev; for reproducible builds, use the same upstream development revision as the bundled source.

```bash
cd Plugins/ProtobufRuntime/Source/ThirdParty
git clone https://github.com/protocolbuffers/protobuf.git protobuf --recursive
```

Expected layout after cloning:
```
ThirdParty/protobuf/
├── src/google/protobuf/      ← headers + .cc sources
├── third_party/abseil-cpp/   ← absl dependency
└── third_party/utf8_range/   ← UTF-8 validation
```

Then right-click the `.uproject` → **Generate Visual Studio project files**, and build.

---

## Blueprint API

### Writer — `UProtobufWriter`

#### `Create`
Creates an empty message ready for field assignment.

| Pin | Type | Description |
|-----|------|-------------|
| ProtoRootDir | String | Directory containing your `.proto` files |
| ProtoFile | String | Relative path to the `.proto` |
| MessageType | String | Fully-qualified protobuf message name, e.g. `game.PlayerInfo` |
| **OutWriter** | UProtobufWriter | Valid object on success, null on failure |
| **OutError** | String | Error on failure |

`MessageType` is looked up by its protobuf name, not by the `.proto` filename or a generated C++ class name. It is formed from the `package` declaration and the message name:

```proto
package game;
message PlayerInfo {}
```

For this schema, use `game.PlayerInfo`. If the file has no `package`, use `PlayerInfo`. For a nested message such as `message Player { message Inventory {} }`, use `game.Player.Inventory`.

#### Field setters (on the OutWriter object)

| Node | Input | Notes |
|------|-------|-------|
| `SetString(FieldName, Value)` | String | Returns false on name/type mismatch |
| `SetInt32(FieldName, Value)` | Int32 | Returns false on name/type mismatch |
| `SetFloat(FieldName, Value)` | Float | Returns false on name/type mismatch |
| `SetBool(FieldName, Value)` | Bool | Returns false on name/type mismatch |
| `Clear()` | — | Reset all fields to defaults |

#### Serialization

| Node | Description |
|------|-------------|
| `SaveToFile(FilePath, OutError)` | Serialize and write to a `.bin` file |
| `SerializeToBytes(OutData, OutError)` | Serialize to a byte array (for network etc.) |

---

### Reader — `UProtobufReader`

### `CreateFromFile`
Load a `.bin` file from disk and parse it in one call.

| Pin | Type | Description |
|-----|------|-------------|
| ProtoRootDir | String | Directory containing your `.proto` files |
| ProtoFile | String | Relative path to the `.proto`, e.g. `"player_info.proto"` |
| MessageType | String | Fully-qualified message name, e.g. `"game.PlayerInfo"` |
| BinFilePath | String | Absolute path to the serialized binary file |
| **OutReader** | UProtobufReader | Valid object on success, null on failure |
| **OutError** | String | Error message on failure, empty on success |

### `CreateFromBytes`
Parse an already-loaded byte array. Same pins as `CreateFromFile` except `BinFilePath` is replaced by `Data (Byte Array)`. Useful when data arrives from network or memory.

### `LoadFileToBytes`
Read any file from disk into a byte array.

| Pin | Type | Description |
|-----|------|-------------|
| FilePath | String | Absolute path to the file |
| **OutData** | Byte Array | File contents |
| **OutError** | String | Error on failure |
| **Return** | Bool | True on success |

### Field accessors (on the OutReader object)

| Node | Return | Notes |
|------|--------|-------|
| `GetString(FieldName)` | String | Returns `""` if field not found |
| `GetInt32(FieldName)` | Int32 | Returns `0` if field not found |
| `GetFloat(FieldName)` | Float | Returns `0.0` if field not found |
| `GetBool(FieldName)` | Bool | Returns `false` if field not found |
| `HasField(FieldName)` | Bool | Schema presence check |
| `GetFieldNames()` | String Array | All fields defined in the message |

---

## Quick start (Blueprint)

**写入**
```
[Create (Writer)]
  ProtoRootDir = "C:/MyProject/Content/Protobuf"
  ProtoFile    = "player_info.proto"
  MessageType  = "game.PlayerInfo"
        |
   OutWriter ──→ SetString "player_name"    "NewPlayer"
             ──→ SetInt32  "level"          10
             ──→ SetFloat  "health"         500.0
             ──→ SetBool   "is_online"      true
             ──→ SaveToFile "C:/Save/player.bin"
```

**读取**
```
[CreateFromFile]
  ProtoRootDir  = "C:/MyProject/Content/Protobuf"
  ProtoFile     = "player_info.proto"
  MessageType   = "game.PlayerInfo"
  BinFilePath   = "C:/MyProject/Content/Protobuf/player_1.bin"
        |
   OutReader ──→ [GetString] FieldName="player_name"  → "HeroKnight"
             ──→ [GetInt32]  FieldName="level"         → 42
             ──→ [GetFloat]  FieldName="health"        → 850.5
             ──→ [GetBool]   FieldName="is_online"     → true
   OutError  ──→ (empty = OK)
```

If `OutReader` is null, print `OutError` to see what went wrong.

---

## Quick start (C++)

```cpp
#include "ProtobufReader.h"

FString Error;
UProtobufReader* Reader = nullptr;

UProtobufReader::CreateFromFile(
    TEXT("C:/MyProject/Content/Protobuf"),
    TEXT("player_info.proto"),
    TEXT("game.PlayerInfo"),
    TEXT("C:/MyProject/Content/Protobuf/player_1.bin"),
    Reader,
    Error);

if (!Reader)
{
    UE_LOG(LogTemp, Error, TEXT("Protobuf load failed: %s"), *Error);
    return;
}

FString Name   = Reader->GetString(TEXT("player_name"));  // "HeroKnight"
int32   Level  = Reader->GetInt32(TEXT("level"));          // 42
float   HP     = Reader->GetFloat(TEXT("health"));         // 850.5
bool    Online = Reader->GetBool(TEXT("is_online"));       // true
```

---

## Generating test data (Python)

Requires `grpcio-tools`:
```bash
pip install grpcio-tools
python gen_proto_testdata.py   # at the project root
```

Generates in `Content/Protobuf/`:
- `player_info.proto` + `player_1/2/3.bin` — player snapshots (name, level, health, score …)
- `room_config.proto` + `room_1/2/3.bin` — room/level configs (map, gravity, time limit …)

---

## Adding your own .proto

1. Write a `proto3` schema with `int32`, `float`, `string`, `bool` fields
2. Place it in any directory accessible at runtime
3. Serialize binary data from Python / Go / any protobuf library
4. Call `CreateFromFile` or `CreateFromBytes`

No engine rebuild required — schemas are loaded entirely at runtime.

---

## Supported field types

| proto3 type | Blueprint node |
|-------------|----------------|
| `string` | `GetString` |
| `int32` | `GetInt32` |
| `float` | `GetFloat` |
| `bool` | `GetBool` |

Repeated fields, nested messages, enums, `int64`, `double`, and `bytes` are not currently exposed through the Blueprint API. The underlying `DynamicMessage` supports them — extend `ProtobufReader.h/.cpp` if needed.

---

## Notes

- `UProtobufReader` is a `UObject` — lifetime is managed by GC, don't hold raw pointers across frames without a `UPROPERTY`
- Protobuf headers must stay in `.cpp` files only; never include them in a public `.h` to avoid polluting other UE compilation units
- If your `.proto` imports other `.proto` files, `ProtoRootDir` must contain all of them
- Module named `ProtobufLib` (not `Protobuf`) to avoid conflict with UE5's built-in Protobuf v21.1 used by MetaHuman SDK

---

## Build notes

- `bUseUnity = false` on both modules — required due to `port_def.inc`/`port_undef.inc` per-TU guards in protobuf
- `PCHUsage = NoPCHs` on `ProtobufLib` for the same reason
- All 222 wrapper files suppress a fixed MSVC warning set: `4100 4127 4141 4146 4065 4191 4244 4267 4305 4310 4312 4456 4457 4554 4661 4701 4702 4800 4855 4946`
- DLL export/import: `LIBPROTOBUF_EXPORTS` / `LIBPROTOC_EXPORTS` / `ABSL_BUILD_DLL` as PrivateDefinitions; `PROTOBUF_USE_DLLS` / `ABSL_CONSUME_DLL` as PublicDefinitions

---

## Third-party licenses

| Library | Version | License |
|---------|---------|---------|
| [protobuf](https://github.com/protocolbuffers/protobuf) | 37.0.0-dev | BSD 3-Clause |
| [abseil-cpp](https://github.com/abseil/abseil-cpp) | lts_20240116 | Apache 2.0 |
| [utf8_range](https://github.com/protocolbuffers/utf8_range) | bundled with protobuf | MIT |
