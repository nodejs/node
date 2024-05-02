// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description("KDE JS Test");
// 12.6.1
var count = 0;
do {
  count++;
} while (count < 10);
shouldBe("count", "10");

count = 0;
for (var i = 0; i < 10; i++) {
  if (i == 5)
    break;
  count++;
}
shouldBe("count", "5");

// 12.6.3
count = 0;
for (i = 0; i < 10; i++) {
  count++;
}
shouldBe("count", "10");

// 12.6.4
obj = new Object();
obj.a = 11;
obj.b = 22;

properties = "";
for ( prop in obj )
  properties += (prop + "=" + obj[prop] + ";");

shouldBe("properties", "'a=11;b=22;'");

// now a test verifying the order. not standardized but common.
obj.y = 33;
obj.x = 44;
properties = "";
for ( prop in obj )
  properties += prop;
// shouldBe("properties", "'abyx'");

arr = new Array;
arr[0] = 100;
arr[1] = 101;
list = "";
for ( var j in arr ) {
  list += "[" + j + "]=" + arr[j] + ";";
}
shouldBe("list","'[0]=100;[1]=101;'");

list = "";
for (var a = [1,2,3], length = a.length, i = 0; i < length; i++) {
  list += a[i];
}
shouldBe("list", "'123'");
