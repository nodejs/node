//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = { i : 1, j : 2, k : 3 };

switch(x.i)
{
        case "1":
                WScript.Echo("error - found \"1\"");
                break;
        default:
                WScript.Echo("error - found default");
                break;
        case 1.000000001:
                WScript.Echo("error - found 1.000000001");
                break;
        case 1:
                WScript.Echo("found 1");
                break;
}

switch(x.q)
{
        case undefined:
                WScript.Echo("found undefined");
                break;
        default:
                WScript.Echo("found a value");
}

x.f = function() { this.j++; return this.j; }
q();
function q() {
        switch(x.j)
        {
                case 1:
                        WScript.Echo("error - found 1");
                        return;
                case x.f():
                        WScript.Echo("error - found x.f()");
                        return;
                case 2:
                        WScript.Echo("found 2, x.j = " + x.j);
                        return;
                case 3:
                        WScript.Echo("error - found 3");
                        return;
        }
}

var y = new Object();
y.z = x;
y.w = x;

switch(x)
{
        case y.w:
                WScript.Echo("found y.w");
                break;
        case y.z:
                WScript.Echo("found y.z");
                break;
}
