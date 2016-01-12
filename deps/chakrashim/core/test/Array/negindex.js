//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//===============================================
// No fast path - because its a negative index
//===============================================
var a = new Array();

a[3221225472] = 3;      // Index 0xC0000000

// non type-specialized case
index = -1073741824;    // Index 0xC0000000, but signed
WScript.Echo(a[index]);

// int const case
WScript.Echo(a[-1073741824]);

// Type Specialized case
var G = 1;
function foo()
{
    var i = 0;
    if (G) i = -1073741824;
    WScript.Echo(a[i]);
}
foo();

//===============================================
// Fast path
//===============================================
var b = new Array();
a[3] = 3;
WScript.Echo(a[3]);
function foo2()
{
    var i = 0;
    if (G) i = 3;
    WScript.Echo(a[i]);
}
foo2();

