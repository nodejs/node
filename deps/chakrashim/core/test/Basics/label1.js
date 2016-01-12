//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function f() {
    // Label in parenthesis is bad syntax, but we allow it. Verify consistency in deferred parsing.
    (a):
        var i = 0;
}

f();

WScript.Echo("pass");
