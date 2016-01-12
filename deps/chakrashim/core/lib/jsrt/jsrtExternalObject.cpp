//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "JsrtPch.h"
#include "JsrtExternalObject.h"
#include "Types\PathTypeHandler.h"

JsrtExternalType::JsrtExternalType(Js::ScriptContext* scriptContext, JsFinalizeCallback finalizeCallback)
    : Js::DynamicType(
        scriptContext,
        Js::TypeIds_Object,
        scriptContext->GetLibrary()->GetObjectPrototype(),
        nullptr,
        Js::SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true),
        true,
        true)
        , jsFinalizeCallback(finalizeCallback)
{
}

JsrtExternalObject::JsrtExternalObject(JsrtExternalType * type, void *data) :
    slot(data),
    Js::DynamicObject(type)
{
}

bool JsrtExternalObject::Is(Js::Var value)
{
    if (Js::TaggedNumber::Is(value))
    {
        return false;
    }

    return (VirtualTableInfo<JsrtExternalObject>::HasVirtualTable(value)) ||
        (VirtualTableInfo<Js::CrossSiteObject<JsrtExternalObject>>::HasVirtualTable(value));
}

JsrtExternalObject * JsrtExternalObject::FromVar(Js::Var value)
{
    Assert(Is(value));
    return static_cast<JsrtExternalObject *>(value);
}

void JsrtExternalObject::Finalize(bool isShutdown)
{
    JsFinalizeCallback finalizeCallback = this->GetExternalType()->GetJsFinalizeCallback();
    if (nullptr != finalizeCallback)
    {
        finalizeCallback(this->slot);
    }
}

void JsrtExternalObject::Dispose(bool isShutdown)
{
}

void * JsrtExternalObject::GetSlotData() const
{
    return this->slot;
}

void JsrtExternalObject::SetSlotData(void * data)
{
    this->slot = data;
}

Js::DynamicType* JsrtExternalObject::DuplicateType()
{
    return RecyclerNew(this->GetScriptContext()->GetRecycler(), JsrtExternalType,
        this->GetExternalType());
}
