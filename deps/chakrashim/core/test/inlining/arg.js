//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo(a, b)
{
    WScript.Echo(a, b);
}

function flap(a,b)
{
    return foo(a+1,b+1);
}

function bar()
{
    flap(1);
    new flap(1);
    flap(10,20);
    new flap(10,20);
    flap(100,200,300);
    new flap(100,200,300);
}

bar();

WScript.Echo("");
bar();
