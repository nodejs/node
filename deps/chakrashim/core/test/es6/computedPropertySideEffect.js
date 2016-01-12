//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function f() 
{
    var v0 = 1;
    var o = { [v0++] : v0 };
    if (o[1] !== 2)
    {
        WScript.Echo('fail1: o[1] === ', o[1]);
    }
}
f();

function g() 
{
    var v0 = 1;
    var o = { [v0++] : v0 };
    function h() { return v0; }
    if (o[1] !== 2)
    {
        WScript.Echo('fail2: o[1] ===', o[1]);
    }
}
g();

function h()
{
    var v0 = 1;
    var o = { [v0] : v0 = 2};
    function h() {}
    if (o[1] !== 2)
    {
        WScript.Echo('fail3: o[1] === ', o[1]);
    }
}
h();

WScript.Echo('pass');    