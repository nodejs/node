//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// VSO OS Bug 1015467
// SimpleTypeHandler::ConvertToTypeHandler incorrectly assumed that all
// property slot values would be proper non-null Var values.  This is
// not true for InternalPropertyId values -- they can be anything, Var
// and non-Var.  Test this code path by utilizing WeakMap, which adds
// an InternalPropertyId to an object.  Then convert the object by
// adding another property.
function f() {
    var o = Object.create(Object.prototype);
    var w = new WeakMap();
    w.set(o, {});
    Object.keys(o);
    o.aaa = "bbb";

    WScript.Echo("Pass");
}
f();
