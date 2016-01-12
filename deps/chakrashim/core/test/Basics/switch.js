//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = { i : 0, j : 1 };
x.f = function(q) {
    WScript.Echo("x.f(" + q + ")");
    this.j++;
    return q;
}

switch (x.i) {
    default:
        WScript.Echo("default");
        break;
    case x.f(1.0):
        WScript.Echo(1.0);
        break;
    case x.f(x.i):
    case x.f(j):
        WScript.Echo(x.i);
        break;
}

switch (x.j) {
    default:
    case "melon":
        WScript.Echo("melon?");
        break;
    case x.f(0):
        WScript.Echo("0");
        break;
}   

WScript.Echo("x.i = " + x.i);
WScript.Echo("x.j = " + x.j);

switch(Math.sqrt(x.i)) {
    case Math.cos(x.j):
        break;
    case 1 ? 2 : 3:
        break;
    case "melon":
        break;
    default:
        WScript.Echo('here we are');
}

(function()
{
    var f = 0;
    switch (f)
    {
        case ((f = 1)? 0 : 0):
            WScript.Echo("pass");
            break;
        default:
            WScript.Echo("fail");
            break;
    };
})();
