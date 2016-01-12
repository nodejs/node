//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var buf1 = new ArrayBuffer(1<<21);
new Int8Array(buf1)[0] = 42;
new Int8Array(buf1)[1<<21-1] = 43;

var buf2 = ArrayBuffer.transfer(buf1, 1<<22);
print(buf1.byteLength);
print(buf2.byteLength);
print(new Int8Array(buf2)[1<<21-1]);
print(new Int8Array(buf2)[0]);
new Int8Array(buf2)[1<<22-1] = 44;
print(new Int8Array(buf2)[1<<22-1]);
var buf3 = ArrayBuffer.transfer(buf2, 1<<20);
print(buf2.byteLength);
print(buf3.byteLength);
print(new Int8Array(buf3)[1<<21-1]);
print(new Int8Array(buf3)[0]);
var buf4 = ArrayBuffer.transfer(buf3, 80);
print(buf3.byteLength);
print(buf4.byteLength);
print(new Int8Array(buf4)[0]);
var buf5 = ArrayBuffer.transfer(buf4, 0);
print(buf4.byteLength);
print(buf5.byteLength);
var buf6 = ArrayBuffer.transfer(buf5, 1<<21);
print(buf5.byteLength);
print(buf6.byteLength);
var buf7 = ArrayBuffer.transfer(buf6, 0);
print(buf6.byteLength);
print(buf7.byteLength);
