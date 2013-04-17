// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"use strict";

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Array = global.Array;

var $ArrayBuffer = global.__ArrayBuffer;

// -------------------------------------------------------------------

function ArrayBufferConstructor(byteLength) { // length = 1
  if (%_IsConstructCall()) {
    var l = TO_POSITIVE_INTEGER(byteLength);
    %ArrayBufferInitialize(this, l);
  } else {
    return new $ArrayBuffer(byteLength);
  }
}

function ArrayBufferGetByteLength() {
  if (!IS_ARRAYBUFFER(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['ArrayBuffer.prototype.byteLength', this]);
  }
  return %ArrayBufferGetByteLength(this);
}

// ES6 Draft 15.13.5.5.3
function ArrayBufferSlice(start, end) {
  if (!IS_ARRAYBUFFER(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['ArrayBuffer.prototype.slice', this]);
  }

  var relativeStart = TO_INTEGER(start);
  var first;
  if (relativeStart < 0) {
    first = MathMax(this.byteLength + relativeStart, 0);
  } else {
    first = MathMin(relativeStart, this.byteLength);
  }
  var relativeEnd = IS_UNDEFINED(end) ? this.byteLength : TO_INTEGER(end);
  var fin;
  if (relativeEnd < 0) {
    fin = MathMax(this.byteLength + relativeEnd, 0);
  } else {
    fin = MathMin(relativeEnd, this.byteLength);
  }

  var newLen = fin - first;
  // TODO(dslomov): implement inheritance
  var result = new $ArrayBuffer(newLen);

  %ArrayBufferSliceImpl(this, result, first);
  return result;
}

// --------------- Typed Arrays ---------------------

function CreateTypedArrayConstructor(name, elementSize, arrayId, constructor) {
  return function (buffer, byteOffset, length) {
    if (%_IsConstructCall()) {
      if (!IS_ARRAYBUFFER(buffer)) {
        throw MakeTypeError("Type error!");
      }
      var offset = IS_UNDEFINED(byteOffset)
        ? 0 : offset = TO_POSITIVE_INTEGER(byteOffset);

      if (offset % elementSize !== 0) {
        throw MakeRangeError("invalid_typed_array_alignment",
            "start offset", name, elementSize);
      }
      var bufferByteLength = %ArrayBufferGetByteLength(buffer);
      if (offset >= bufferByteLength) {
        throw MakeRangeError("invalid_typed_array_offset");
      }

      var newByteLength;
      var newLength;
      if (IS_UNDEFINED(length)) {
        if (bufferByteLength % elementSize !== 0) {
          throw MakeRangeError("invalid_typed_array_alignment",
            "byte length", name, elementSize);
        }
        newByteLength = bufferByteLength - offset;
        newLength = newByteLength / elementSize;
      } else {
        var newLength = TO_POSITIVE_INTEGER(length);
        newByteLength = newLength * elementSize;
      }
      if (newByteLength > bufferByteLength) {
        throw MakeRangeError("invalid_typed_array_length");
      }
      %TypedArrayInitialize(this, arrayId, buffer, offset, newByteLength);
    } else {
      return new constructor(buffer, byteOffset, length);
    }
  }
}

function TypedArrayGetBuffer() {
  return %TypedArrayGetBuffer(this);
}

function TypedArrayGetByteLength() {
  return %TypedArrayGetByteLength(this);
}

function TypedArrayGetByteOffset() {
  return %TypedArrayGetByteOffset(this);
}

function TypedArrayGetLength() {
  return %TypedArrayGetLength(this);
}


// -------------------------------------------------------------------

function SetUpArrayBuffer() {
  %CheckIsBootstrapping();

  // Set up the ArrayBuffer constructor function.
  %SetCode($ArrayBuffer, ArrayBufferConstructor);
  %FunctionSetPrototype($ArrayBuffer, new $Object());

  // Set up the constructor property on the ArrayBuffer prototype object.
  %SetProperty($ArrayBuffer.prototype, "constructor", $ArrayBuffer, DONT_ENUM);

  InstallGetter($ArrayBuffer.prototype, "byteLength", ArrayBufferGetByteLength);

  InstallFunctions($ArrayBuffer.prototype, DONT_ENUM, $Array(
      "slice", ArrayBufferSlice
  ));
}

SetUpArrayBuffer();

function SetupTypedArray(arrayId, name, constructor, elementSize) {
  var f = CreateTypedArrayConstructor(name, elementSize,
                                      arrayId, constructor);
  %SetCode(constructor, f);
  %FunctionSetPrototype(constructor, new $Object());

  %SetProperty(constructor.prototype,
               "constructor", constructor, DONT_ENUM);
  %SetProperty(constructor.prototype,
               "BYTES_PER_ELEMENT", elementSize,
               READ_ONLY | DONT_ENUM | DONT_DELETE);
  InstallGetter(constructor.prototype, "buffer", TypedArrayGetBuffer);
  InstallGetter(constructor.prototype, "byteOffset", TypedArrayGetByteOffset);
  InstallGetter(constructor.prototype, "byteLength", TypedArrayGetByteLength);
  InstallGetter(constructor.prototype, "length", TypedArrayGetLength);
}

// arrayIds below should be synchronized with Runtime_TypedArrayInitialize.
SetupTypedArray(1, "Uint8Array", global.__Uint8Array, 1);
SetupTypedArray(2, "Int8Array", global.__Int8Array, 1);
SetupTypedArray(3, "Uint16Array", global.__Uint16Array, 2);
SetupTypedArray(4, "Int16Array", global.__Int16Array, 2);
SetupTypedArray(5, "Uint32Array", global.__Uint32Array, 4);
SetupTypedArray(6, "Int32Array", global.__Int32Array, 4);
SetupTypedArray(7, "Float32Array", global.__Float32Array, 4);
SetupTypedArray(8, "Float64Array", global.__Float64Array, 8);

