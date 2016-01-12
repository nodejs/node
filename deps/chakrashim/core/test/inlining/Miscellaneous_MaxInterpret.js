//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// 'arguments[0] = ...' needs to kill 'o' and in turn, 'o.p' as well
function test0() {
    var o = { p: 0 };
    var func0 = function(o) {
        ++o.p;
        arguments[0] = "x";
        ++o.p;
    }
    func0(o);
    return o.p;
};
WScript.Echo("test0: " + test0());
WScript.Echo("test0: " + test0());
