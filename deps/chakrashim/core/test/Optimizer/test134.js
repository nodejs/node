//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    (function() { })(!(o.p *= x) ? Math.atan(!(o.p *= x)) : 1);
}
var o = {};
var x = 1;
o.p = 100;
o.p = test0();
test0();
WScript.Echo(o.p);
