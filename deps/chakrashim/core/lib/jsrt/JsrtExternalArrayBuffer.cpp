//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "JsrtPch.h"
#include "JsrtExternalArrayBuffer.h"

namespace Js
{
    JsrtExternalArrayBuffer::JsrtExternalArrayBuffer(byte *buffer, uint32 length, JsFinalizeCallback finalizeCallback, void *callbackState, DynamicType *type)
        : ExternalArrayBuffer(buffer, length, type), finalizeCallback(finalizeCallback), callbackState(callbackState)
    {
    }

    JsrtExternalArrayBuffer* JsrtExternalArrayBuffer::New(byte *buffer, uint32 length, JsFinalizeCallback finalizeCallback, void *callbackState, DynamicType *type)
    {
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        return RecyclerNewFinalized(recycler, JsrtExternalArrayBuffer, buffer, length, finalizeCallback, callbackState, type);
    }

    void JsrtExternalArrayBuffer::Finalize(bool isShutdown)
    {
        if (finalizeCallback != nullptr)
        {
            finalizeCallback(callbackState);
        }
    }
}
