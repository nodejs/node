// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var buffer1 = new ArrayBuffer(100 * 1024);

assertThrows(function() {
  var array1 = new Uint8Array(buffer1, {valueOf : function() {
    %ArrayBufferDetach(buffer1);
    return 0;
  }});
}, TypeError);

var buffer2 = new ArrayBuffer(100 * 1024);

assertThrows(function() {
  var array2 = new Uint8Array(buffer2, 0, {valueOf : function() {
      %ArrayBufferDetach(buffer2);
      return 100 * 1024;
  }});
}, TypeError);

let convertedOffset = false;
let convertedLength = false;
assertThrows(() =>
  new Uint8Array(buffer1, {valueOf : function() {
      convertedOffset = true;
      return 0;
    }}, {valueOf : function() {
      convertedLength = true;
      %ArrayBufferDetach(buffer1);
      return 0;
  }}), TypeError);
assertTrue(convertedOffset);
assertTrue(convertedLength);

var buffer5 = new ArrayBuffer(100 * 1024);
assertThrows(function() {
  buffer5.slice({valueOf : function() {
    %ArrayBufferDetach(buffer5);
    return 0;
  }}, 100 * 1024 * 1024);
}, TypeError);


var buffer7 = new ArrayBuffer(100 * 1024 * 1024);
assertThrows(function() {
  buffer7.slice(0, {valueOf : function() {
    %ArrayBufferDetach(buffer7);
    return 100 * 1024 * 1024;
  }});
}, TypeError);

var buffer9 = new ArrayBuffer(1024);
var array9 = new Uint8Array(buffer9);
assertThrows(() =>
  array9.subarray({valueOf : function() {
    %ArrayBufferDetach(buffer9);
    return 0;
  }}, 1024), TypeError);
assertEquals(0, array9.length);

var buffer11 = new ArrayBuffer(1024);
var array11 = new Uint8Array(buffer11);
assertThrows(() =>
  array11.subarray(0, {valueOf : function() {
    %ArrayBufferDetach(buffer11);
    return 1024;
  }}), TypeError);
assertEquals(0, array11.length);
