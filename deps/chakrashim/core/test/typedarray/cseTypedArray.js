//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo(arr,y,a)
{
    var x = arr[y] |0;
    var z = arr[y];
    var b = arr[y];
    WScript.Echo(""+z);
    WScript.Echo(""+b);
    var m = z + b + x;
    return m;

}
var buffer = new ArrayBuffer(1<<16);
var heapArr = new Int8Array(buffer);
heapArr[10] = 100;
print(foo(heapArr,10,1));
print(foo(heapArr,10,1));
print(isNaN(foo(heapArr,1<<17,1)));

