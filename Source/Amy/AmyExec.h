// Copyright DJ Song Super-Star. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"

class FAmyExec: public FSelfRegisteringExec
{	
public:
	FAmyExec();

protected:
	// Begin FExec Interface
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	// End FExec Interface
};

