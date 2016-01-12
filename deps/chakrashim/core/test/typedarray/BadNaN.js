//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var arrayBuf = new ArrayBuffer(8);

var float64Array = new Float64Array(arrayBuf);
var float32Array = new Float32Array(arrayBuf);
var int32Array = new Int32Array(arrayBuf);

int32Array[0] = -1;
int32Array[1] = -1;

function FAIL()
{
    WScript.Echo("FAILED\n");
    throw 0;
}

function foo1(i)
{
    return float32Array[i];
}
function foo2(i)
{
    return float64Array[i];
}

var x;

for (var i = 0; i < 1000; i++)
{
    x = foo1(0);
    if (x === x) FAIL();

    x = foo2(0);
    if (x === x) FAIL();
}

WScript.Echo("Passed\n");
