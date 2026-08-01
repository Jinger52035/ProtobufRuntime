#pragma once

#include "Modules/ModuleManager.h"

class FProtobufRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
