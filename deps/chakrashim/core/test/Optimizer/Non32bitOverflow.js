//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function Test0Compound(a, b)
{
    var t0, t1;
    t0 = a * b;
    d = t0 & a;
    return d;
}

function Test1Compound(a, b)
{
    var t0, t1, t2, t3;
    t0 = a * b;
    t2 = t0 + a;
    d = t2 & a;
    return d;
}


function Test2Compound(a, b)
{
    var t0, t1, t2, t3;

    t0 = a * b;
    t2 = t0 + a;
    t2 = t2 + b;
    d = t2 & a;
    return d;
}


function Test16Compound(a, b)
{
    var t0, t1, t2, t3;

    t0 = a * b;
    // 8
    t2 = t0 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    // 8 
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    
    d = t2 & a;
    return d;
}

function Test20Compound(a, b)
{
    var t0, t1, t2, t3;

    t0 = a * b;
    // 8
    t2 = t0 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    // 8 
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    // 4 
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;

    
    d = t2 & a;
    return d;
}


function Test32Compound(a, b)
{
    var t0, t1, t2, t3;

    t0 = a * b;
    // 8
    t2 = t0 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    // 8 
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    // 8
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    // 8
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;
    t2 = t2 + a;
    t2 = t2 + b;

    
    d = t2 & a;
    return d;
}


for (var i = 1; i < 2147483647; i+=3131313)
{
    
    WScript.Echo("Test0Compound(" + -i + ", " + i + ") = " +  Test0Compound(-i, i));
    WScript.Echo("Test0Compound(" + i + ", " + i + ") = " +  Test0Compound(i, i));
    
    WScript.Echo("Test1Compound(" + -i + ", " + i + ") = " +   Test1Compound(-i, i));
    WScript.Echo("Test1Compound(" + i  + ", " + i + ") = " +   Test1Compound(i, i));
    
    WScript.Echo("Test2Compound(" + -i + ", " + i + ") = " +   Test2Compound(-i, i));
    WScript.Echo("Test2Compound(" + i  + ", " + i + ") = " +   Test2Compound(i, i));
    
    WScript.Echo("Test16Compound(" + -i + ", " + i + ") = " +   Test16Compound(-i, i));
    WScript.Echo("Test16Compound(" + i  + ", " + i + ") = " +   Test16Compound(i, i));
    
    WScript.Echo("Test20Compound(" + -i + ", " + i + ") = " +   Test20Compound(-i, i));
    WScript.Echo("Test20Compound(" + i  + ", " + i + ") = " +   Test20Compound(i, i));
    
    WScript.Echo("Test32Compound(" + -i + ", " + i + ") = " +   Test32Compound(-i, i));
    WScript.Echo("Test32Compound(" + i  + ", " + i + ") = " +   Test32Compound(i, i));
}
    
    
WScript.Echo(Test1Compound(2147483647, 2147483647));
WScript.Echo(Test1Compound(-2147483648, 2147483647));
WScript.Echo(Test1Compound(2147483647, -2147483648));

WScript.Echo(Test2Compound(2147483647, 2147483647));
WScript.Echo(Test2Compound(-2147483648, 2147483647));
WScript.Echo(Test2Compound(2147483647, -2147483648));

WScript.Echo(Test16Compound(2147483647, 2147483647));
WScript.Echo(Test16Compound(-2147483648, 2147483647));
WScript.Echo(Test16Compound(2147483647, -2147483648));

WScript.Echo(Test20Compound(2147483647, 2147483647));
WScript.Echo(Test20Compound(-2147483648, 2147483647));
WScript.Echo(Test20Compound(2147483647, -2147483648));

WScript.Echo(Test32Compound(2147483647, 2147483647));
WScript.Echo(Test32Compound(-2147483648, 2147483647));
WScript.Echo(Test32Compound(2147483647, -2147483648));



