// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

var $ArrayBuffer = global.ArrayBuffer;

// -------------------------------------------------------------------

function ArrayBufferConstructor(length) { // length = 1
  if (%_IsConstructCall()) {
    var byteLength = ToPositiveInteger(length, 'invalid_array_buffer_length');
    %ArrayBufferInitialize(this, byteLength);
  } else {
    throw MakeTypeError('constructor_not_function', ["ArrayBuffer"]);
  }
}

function ArrayBufferGetByteLen() {
  if (!IS_ARRAYBUFFER(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['ArrayBuffer.prototype.byteLength', this]);
  }
  return %_ArrayBufferGetByteLength(this);
}

// ES6 Draft 15.13.5.5.3
function ArrayBufferSlice(start, end) {
  if (!IS_ARRAYBUFFER(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['ArrayBuffer.prototype.slice', this]);
  }

  var relativeStart = TO_INTEGER(start);
  if (!IS_UNDEFINED(end)) {
    end = TO_INTEGER(end);
  }
  var first;
  var byte_length = %_ArrayBufferGetByteLength(this);
  if (relativeStart < 0) {
    first = MathMax(byte_length + relativeStart, 0);
  } else {
    first = MathMin(relativeStart, byte_length);
  }
  var relativeEnd = IS_UNDEFINED(end) ? byte_length : end;
  var fin;
  if (relativeEnd < 0) {
    fin = MathMax(byte_length + relativeEnd, 0);
  } else {
    fin = MathMin(relativeEnd, byte_length);
  }

  if (fin < first) {
    fin = first;
  }
  var newLen = fin - first;
  // TODO(dslomov): implement inheritance
  var result = new $ArrayBuffer(newLen);

  %ArrayBufferSliceImpl(this, result, first);
  return result;
}

function ArrayBufferIsViewJS(obj) {
  return %ArrayBufferIsView(obj);
}

function SetUpArrayBuffer() {
  %CheckIsBootstrapping();

  // Set up the ArrayBuffer constructor function.
  %SetCode($ArrayBuffer, ArrayBufferConstructor);
  %FunctionSetPrototype($ArrayBuffer, new $Object());

  // Set up the constructor property on the ArrayBuffer prototype object.
  %AddNamedProperty(
      $ArrayBuffer.prototype, "constructor", $ArrayBuffer, DONT_ENUM);

  %AddNamedProperty($ArrayBuffer.prototype,
      symbolToStringTag, "ArrayBuffer", DONT_ENUM | READ_ONLY);

  InstallGetter($ArrayBuffer.prototype, "byteLength", ArrayBufferGetByteLen);

  InstallFunctions($ArrayBuffer, DONT_ENUM, $Array(
      "isView", ArrayBufferIsViewJS
  ));

  InstallFunctions($ArrayBuffer.prototype, DONT_ENUM, $Array(
      "slice", ArrayBufferSlice
  ));
}

SetUpArrayBuffer();
