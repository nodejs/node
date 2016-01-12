//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

namespace Js
{
    BreakpointProbe::BreakpointProbe(DebugDocument* debugDocument, StatementLocation& statement) :
        debugDocument(debugDocument),
        functionBody(statement.function),
        characterOffset(statement.statement.begin),
        byteOffset(statement.bytecodeSpan.begin)
    {
    }

    bool BreakpointProbe::Install(ScriptContext* pScriptContext)
    {
        Assert(this->functionBody);
        return functionBody->InstallProbe(byteOffset);
    }

    bool BreakpointProbe::Uninstall(ScriptContext* pScriptContext)
    {
        Assert(this->functionBody);

        if (this->functionBody)
        {
            Assert(this->debugDocument);
            this->debugDocument->RemoveBreakpointProbe(this);

            return functionBody->UninstallProbe(byteOffset);
        }

        return true;
    }

    bool BreakpointProbe::CanHalt(InterpreterHaltState* pHaltState)
    {
        Assert(this->functionBody);

        FunctionBody* pCurrentFuncBody = pHaltState->GetFunction();
        int offset = pHaltState->GetCurrentOffset();

        if (functionBody == pCurrentFuncBody && byteOffset == offset)
        {
            return true;
        }
        return false;
    }

    void BreakpointProbe::DispatchHalt(InterpreterHaltState* pHaltState)
    {
        Assert(false);
    }

    void BreakpointProbe::CleanupHalt()
    {
        Assert(this->functionBody);

        // Nothing to clean here
    }

    bool BreakpointProbe::Matches(FunctionBody* _pBody, int _characterOffset)
    {
        Assert(this->functionBody);
        return _pBody == functionBody && _characterOffset == characterOffset;
    }
}
