//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function a(o)
{
    o.x = 1;
    WScript.Echo(o.x);
}
function b(o)
{
    o.x = 1;
    WScript.Echo(o.x);
}
function c(o)
{
    o.x = 1;
    WScript.Echo(o.x);
}
function d(o)
{
    o.x = 1;
    WScript.Echo(o.x);
}
function e(o)
{
    o.x = 1;
    WScript.Echo(o.x);
}
function f(o)
{
    o.x = 1;
    WScript.Echo(o.x);
}

a(1);
b(1.1);
c(true);
d(false);
e("blah");
f(new Object());
