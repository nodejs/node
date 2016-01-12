//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function module(stdlib)
{
    "use asm"
    var fr = stdlib.Math.fround;
    function foo()
    {
        var s = fr(10.5);
        var i = fr(20);
        var g = fr(30);
        var h = fr(42.42);
        var j = fr(19.5);
        var c = 125.5

        i = fr(s - i);
        g = fr(g/s);
        c = +j;
        h = fr(s*h);
        s =   fr(+j);
        return fr(fr(s + i) +fr( g + h ));
    }
    return foo;
}
var x = module({Math:Math});
WScript.Echo(x());
WScript.Echo(x());
