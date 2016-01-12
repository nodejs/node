//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {
    class JsrtExternalArrayBuffer : public ExternalArrayBuffer
    {
    protected:
        DEFINE_VTABLE_CTOR(JsrtExternalArrayBuffer, ExternalArrayBuffer);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JsrtExternalArrayBuffer);

        JsrtExternalArrayBuffer(byte *buffer, uint32 length, JsFinalizeCallback finalizeCallback, void *callbackState, DynamicType *type);

    public:
        static JsrtExternalArrayBuffer* New(byte *buffer, uint32 length, JsFinalizeCallback finalizeCallback, void *callbackState, DynamicType *type);
        void Finalize(bool isShutdown) override;

    private:
        JsFinalizeCallback finalizeCallback;
        void *callbackState;
    };
    AUTO_REGISTER_RECYCLER_OBJECT_DUMPER(JsrtExternalArrayBuffer, &Js::RecyclableObject::DumpObjectFunction);
}
