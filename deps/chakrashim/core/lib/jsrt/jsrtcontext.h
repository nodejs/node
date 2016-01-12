//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "JsrtRuntime.h"

class JsrtContext : public FinalizableObject
{
public:
    static JsrtContext *New(JsrtRuntime * runtime);

    Js::ScriptContext * GetScriptContext() const { return this->scriptContext; }
    JsrtRuntime * GetRuntime() const { return this->runtime; }
    void* GetExternalData() const { return this->externalData; }
    void SetExternalData(void * data) { this->externalData = data; }

    static bool Initialize();
    static void Uninitialize();
    static JsrtContext * GetCurrent();
    static bool TrySetCurrent(JsrtContext * context);
    static bool Is(void * ref);

    virtual void Finalize(bool isShutdown) override sealed;
    virtual void Mark(Recycler * recycler) override sealed;

    void OnScriptLoad(Js::JavascriptFunction * scriptFunction, Js::Utf8SourceInfo* utf8SourceInfo);
protected:
    DEFINE_VTABLE_CTOR_NOBASE(JsrtContext);
    JsrtContext(JsrtRuntime * runtime);
    void Link();
    void Unlink();
    void SetScriptContext(Js::ScriptContext * scriptContext);
    void PinCurrentJsrtContext();
private:
    static DWORD s_tlsSlot;
    Js::ScriptContext * scriptContext;
    JsrtRuntime * runtime;
    void * externalData = nullptr;
    JsrtContext * previous;
    JsrtContext * next;
};

