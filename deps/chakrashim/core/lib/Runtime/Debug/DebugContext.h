//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class HostDebugContext
{
public:
    HostDebugContext(Js::ScriptContext* inScriptContext) { this->scriptContext = inScriptContext; }
    virtual void Delete() = 0;
    virtual DWORD_PTR GetHostSourceContext(Js::Utf8SourceInfo * sourceInfo) = 0;
    virtual HRESULT SetThreadDescription(__in LPCWSTR url) = 0;
    virtual HRESULT DbgRegisterFunction(Js::ScriptContext * scriptContext, Js::FunctionBody * functionBody, DWORD_PTR dwDebugSourceContext, LPCWSTR title) = 0;
    virtual void ReParentToCaller(Js::Utf8SourceInfo* sourceInfo) = 0;

    Js::ScriptContext* GetScriptContext() { return scriptContext; }

private:
    Js::ScriptContext* scriptContext;
};

namespace Js
{
    // Represents the different modes that the debugger can be placed into.
    enum DebuggerMode
    {
        // The debugger is not running so the engine can be running
        // in JITed mode.
        NotDebugging,

        // The debugger is not running but PDM has been created and
        // source rundown was performed to register script documents.
        SourceRundown,

        // The debugger is running which means that the engine is
        // running in interpreted mode.
        Debugging,
    };

    class DebugContext
    {
    public:
        DebugContext(Js::ScriptContext * scriptContext);
        ~DebugContext();
        void Initialize();
        HRESULT RundownSourcesAndReparse(bool shouldPerformSourceRundown, bool shouldReparseFunctions);
        void RegisterFunction(Js::ParseableFunctionInfo * func, LPCWSTR title);
        void Close();
        void SetHostDebugContext(HostDebugContext * hostDebugContext);

        DebuggerMode GetDebuggerMode() const { return this->debuggerMode; }
        void SetDebuggerMode(DebuggerMode mode);
        void SetInDebugMode() { this->SetDebuggerMode(DebuggerMode::Debugging); }
        void SetInSourceRundownMode() { this->SetDebuggerMode(DebuggerMode::SourceRundown); }

        bool IsInNonDebugMode() const { return this->GetDebuggerMode() == DebuggerMode::NotDebugging; }
        bool IsInSourceRundownMode() const { return this->GetDebuggerMode() == DebuggerMode::SourceRundown; }
        bool IsInDebugMode() const { return this->GetDebuggerMode() == DebuggerMode::Debugging; }
        bool IsInDebugOrSourceRundownMode() const { return this->IsInDebugMode() || this->IsInSourceRundownMode(); }

        ProbeContainer* GetProbeContainer() const { return this->diagProbesContainer; }

    private:
        ScriptContext * scriptContext;
        HostDebugContext* hostDebugContext;
        DebuggerMode debuggerMode;
        ProbeContainer* diagProbesContainer;

        // Private Functions
        void FetchTopLevelFunction(JsUtil::List<Js::FunctionBody *, ArenaAllocator>* pFunctions, Js::Utf8SourceInfo * sourceInfo);
        void WalkAndAddUtf8SourceInfo(Js::Utf8SourceInfo* sourceInfo, JsUtil::List<Js::Utf8SourceInfo *, Recycler, false, Js::CopyRemovePolicy, RecyclerPointerComparer> *utf8SourceInfoList);
        bool CanRegisterFunction() const;
        void RegisterFunction(Js::FunctionBody * functionBody, DWORD_PTR dwDebugSourceContext, LPCWSTR title);
        HostDebugContext * GetHostDebugContext() const { return hostDebugContext; }

        template<class TMapFunction>
        void MapUTF8SourceInfoUntil(TMapFunction map);
    };
}
