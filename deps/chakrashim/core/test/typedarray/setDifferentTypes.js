//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var arr = new Int8Array([-256,255,0,-0,NaN,null,undefined,-1,256,-128,-127,127,128]);
var dst1 = new Uint8ClampedArray(arr.length);
var dst2 = new Uint8Array(arr.length);
var arr2 = new Uint8Array([-256,255,0,-0,NaN,null,undefined,-1,256,-128,-127,127,128]);
var dst3 = new Int8Array(arr2.length);
var dst4 = new Int32Array(arr2.length);

dst1.set(arr);
dst2.set(arr);
dst3.set(arr);
dst4.set(arr);
for (i = 0; i < dst1.length; i++) {
    WScript.Echo("dst1[" + i + "] = " + dst1[i]);
}
for (i = 0; i < dst2.length; i++) {
    WScript.Echo("dst2[" + i + "] = " + dst2[i]);
}
for (i = 0; i < dst3.length; i++) {
    WScript.Echo("dst3[" + i + "] = " + dst3[i]);
}
for (i = 0; i < dst4.length; i++) {
    WScript.Echo("dst4[" + i + "] = " + dst4[i]);
}