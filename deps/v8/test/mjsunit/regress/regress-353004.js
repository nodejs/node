// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var buffer1 = new ArrayBuffer(100 * 1024);

var array1 = new Uint8Array(buffer1, {valueOf : function() {
  %ArrayBufferNeuter(buffer1);
  return 0;
}});

assertEquals(0, array1.length);

var buffer2 = new ArrayBuffer(100 * 1024);

assertThrows(function() {
  var array2 = new Uint8Array(buffer2, 0, {valueOf : function() {
      %ArrayBufferNeuter(buffer2);
      return 100 * 1024;
  }});
}, RangeError);


var buffer3 = new ArrayBuffer(100 * 1024 * 1024);
var dataView1 = new DataView(buffer3, {valueOf : function() {
  %ArrayBufferNeuter(buffer3);
  return 0;
}});

assertEquals(0, dataView1.byteLength);

var buffer4 = new ArrayBuffer(100 * 1024);
assertThrows(function() {
  var dataView2 = new DataView(buffer4, 0, {valueOf : function() {
    %ArrayBufferNeuter(buffer4);
    return 100 * 1024 * 1024;
  }});
}, RangeError);


var buffer5 = new ArrayBuffer(100 * 1024);
assertThrows(function() {
  buffer5.slice({valueOf : function() {
    %ArrayBufferNeuter(buffer5);
    return 0;
  }}, 100 * 1024 * 1024);
}, TypeError);


var buffer7 = new ArrayBuffer(100 * 1024 * 1024);
assertThrows(function() {
  buffer7.slice(0, {valueOf : function() {
    %ArrayBufferNeuter(buffer7);
    return 100 * 1024 * 1024;
  }});
}, TypeError);

var buffer9 = new ArrayBuffer(1024);
var array9 = new Uint8Array(buffer9);
var array10 = array9.subarray({valueOf : function() {
    %ArrayBufferNeuter(buffer9);
    return 0;
  }}, 1024);
assertEquals(0, array9.length);
assertEquals(0, array10.length);

var buffer11 = new ArrayBuffer(1024);
var array11 = new Uint8Array(buffer11);
var array12 = array11.subarray(0, {valueOf : function() {
      %ArrayBufferNeuter(buffer11);
      return 1024;
    }});
assertEquals(0, array11.length);
assertEquals(0, array12.length);
