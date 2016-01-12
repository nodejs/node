//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class RuntimeFunction : public JavascriptFunction
    {
    protected:
        DEFINE_VTABLE_CTOR(RuntimeFunction, JavascriptFunction);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(RuntimeFunction);
        RuntimeFunction(DynamicType * type);
    public:
        RuntimeFunction(DynamicType * type, FunctionInfo * functionInfo);
        RuntimeFunction(DynamicType * type, FunctionInfo * functionInfo, ConstructorCache* cache);

        void SetFunctionNameId(Var nameId);

        // this is for cached source string for the function. Possible values are:
        // NULL; initialized for anonymous methods.
        // propertyId in Int31 format; this is used for fastDOM function as well as library function
        // JavascriptString: composed using functionname from the propertyId, or fixed string for anonymous functions.
        Var functionNameId;
        virtual Var GetSourceString() const { return functionNameId; }
        virtual Var EnsureSourceString();
    };
};
