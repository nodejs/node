// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalArrayBuffer = global.ArrayBuffer;
var GlobalObject = global.Object;

var MathMax;
var MathMin;

utils.Import(function(from) {
  MathMax = from.MathMax;
  MathMin = from.MathMin;
});

// -------------------------------------------------------------------

function ArrayBufferConstructor(length) { // length = 1
  if (%_IsConstructCall()) {
    var byteLength = $toPositiveInteger(length, kInvalidArrayBufferLength);
    %ArrayBufferInitialize(this, byteLength, kNotShared);
  } else {
    throw MakeTypeError(kConstructorNotFunction, "ArrayBuffer");
  }
}

function ArrayBufferGetByteLen() {
  if (!IS_ARRAYBUFFER(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'ArrayBuffer.prototype.byteLength', this);
  }
  return %_ArrayBufferGetByteLength(this);
}

// ES6 Draft 15.13.5.5.3
function ArrayBufferSlice(start, end) {
  if (!IS_ARRAYBUFFER(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'ArrayBuffer.prototype.slice', this);
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
  var result = new GlobalArrayBuffer(newLen);

  %ArrayBufferSliceImpl(this, result, first);
  return result;
}

function ArrayBufferIsViewJS(obj) {
  return %ArrayBufferIsView(obj);
}


// Set up the ArrayBuffer constructor function.
%SetCode(GlobalArrayBuffer, ArrayBufferConstructor);
%FunctionSetPrototype(GlobalArrayBuffer, new GlobalObject());

// Set up the constructor property on the ArrayBuffer prototype object.
%AddNamedProperty(
    GlobalArrayBuffer.prototype, "constructor", GlobalArrayBuffer, DONT_ENUM);

%AddNamedProperty(GlobalArrayBuffer.prototype,
    symbolToStringTag, "ArrayBuffer", DONT_ENUM | READ_ONLY);

utils.InstallGetter(GlobalArrayBuffer.prototype, "byteLength", ArrayBufferGetByteLen);

utils.InstallFunctions(GlobalArrayBuffer, DONT_ENUM, [
  "isView", ArrayBufferIsViewJS
]);

utils.InstallFunctions(GlobalArrayBuffer.prototype, DONT_ENUM, [
  "slice", ArrayBufferSlice
]);

})
