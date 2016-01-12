//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

Object.prototype[5]  = "obj.proto5";
Object.prototype[7]  = "obj.proto7";

Array.prototype[1]   = "arr.proto.1";
Array.prototype[2]   = "arr.proto.2";
Array.prototype[3]   = "arr.proto.3";
Array.prototype[6]   = "arr.proto.6";

var n=8;
var i=0;

var arr = new Array(n);

for (i=3;i<n;i++) { arr[i] = i * i + 1; }

write(delete arr[1]);   write("T1:" + arr.length + " : " + arr);
write(delete arr[3]);   write("T2:" + arr.length + " : " + arr);
write(delete arr[n-1]); write("T3:" + arr.length + " : " + arr);
write(delete arr[n+1]); write("T4:" + arr.length + " : " + arr);