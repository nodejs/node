//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a = 0x40000000;
var b = 0x40000001;
var c  = 0x3fffffff;

test(a,b,c);

a = 0xfffffffe;
b = 0xffffffff;
c = 0x0;

test(a,b,c);

a = 0x40000000;
b = 0x40000001;
c  = 0;

test(a,b,c);

function test(a,b,c)
{

    WScript.Echo(a.toString(16));
    WScript.Echo(b.toString(16));
    WScript.Echo(c.toString(16));

    if(a < b) WScript.Echo("less");
    else      WScript.Echo("greater");

    if(a > b) WScript.Echo("greater");
    else      WScript.Echo("less");

    if(a < c) WScript.Echo("less");
    else      WScript.Echo("greater");

    if(a > c) WScript.Echo("greater");
    else      WScript.Echo("less");

    if(b < c) WScript.Echo("less");
    else      WScript.Echo("greater");

    if(b > c) WScript.Echo("less");
    else      WScript.Echo("greater");
}

