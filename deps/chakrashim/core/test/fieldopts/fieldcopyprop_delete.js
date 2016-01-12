//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Make sure delete kill field copy prop values
function test()
{
    var o = new Object();
    o.x = 1;

    var a = o.x;
    var b = delete o.x;
    var c = o.x;
    var d = delete o.x;

    WScript.Echo(a);
    WScript.Echo(b);
    WScript.Echo(c);
    WScript.Echo(d);

    o.x = 2;
    var a = o.x;
    var b = delete o["x"];
    var c = o.x;
    var d = delete o["x"];
    WScript.Echo(a);
    WScript.Echo(b);
    WScript.Echo(c);
    WScript.Echo(d);

    Object.defineProperty(o, "x", { configurable: false, value : 3 });
    var a = o.x;
    var b = delete o.x;
    var c = o.x;

    WScript.Echo(a);
    WScript.Echo(b);
    WScript.Echo(c);
}

test();
