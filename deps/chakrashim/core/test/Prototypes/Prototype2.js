//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//
// Tests the relationship between a parent and child object.
//

function print(x,str)
{
    WScript.Echo("=== " + str + " ===");
    WScript.Echo("x.q:    " + x.q);
    WScript.Echo("x[3]:   " + x[3]);
    WScript.Echo("x[4]:   " + x[4]);
    WScript.Echo("x[50]:  " + x[50]);
    WScript.Echo("x.p1:   " + x.p1);
    WScript.Echo("x.p2:   " + x.p2);
    WScript.Echo("x[\"m\"]: " + x["m"]);
    WScript.Echo("");
}

var z = new Array(10);

for(var i = 0; i < 10; ++i)
{
    z[i] = i;
}
z.p1 = "test";
z.p2 = 3;

function F(x)
{
    this[x] = 1;
}
F.prototype = z;

var x = new F("q");

print(x, "after object creation");

z.m = 14;
print(x, "after adding new property to parent");

F.prototype = new String("glah");
print(x, "after modifying constructor's prototype");

z.m--;
print(x, "after modifying parent");

z.p1 = undefined;
z[3] = undefined;
z[4] <<= 2;
z[50] = 42;
print(x, "after undefining properties on parent");

z.p1 = new String("new p1");
print(x, "after re-adding property on parent");

x.p1 = "x's p1";
z.p1 = undefined;
print(x, "after re-defining property on object");
