#include "HAL/Platform.h"
#include "Modules/ModuleManager.h"

// This module is a pure C++ library wrapper — no UObject, no IMPLEMENT_MODULE needed.
// UBT requires at least one .cpp to link the module.
IMPLEMENT_MODULE(FDefaultModuleImpl, ProtobufLib)
