//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//****************Test0 creates a SimpleDictionaryTypeHandler****************
var B = 6;

function test0()
{
    return B;
}

WScript.Echo(test0());
//JIT test() with fixedDataProp
WScript.Echo(test0());
B++;
//Should bail out during this call
WScript.Echo(test0());


//****************Test 1 creates a PathTypeHandler****************
var obj = {A:1}

function test1()
{
    return obj.A;
}

WScript.Echo(test1());
WScript.Echo(test1());
obj.A = 2;
//Bails out here, since a new property is added.
WScript.Echo(test1());

//*******************Test2: Creates a DictionaryTypeHandler****************
Object.prototype.C = 5;

function test2()
{
    return C;
}

WScript.Echo(test2());
WScript.Echo(test2());
C=2;
WScript.Echo(test2());



