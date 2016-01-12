//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class HostObjectBase : public DynamicObject
    {
    public:
        virtual Js::ModuleRoot * GetModuleRoot(ModuleID moduleID) = 0;

    protected:
        DEFINE_VTABLE_CTOR(HostObjectBase, DynamicObject);
        HostObjectBase(DynamicType * type) : DynamicObject(type) {};
    };
};
