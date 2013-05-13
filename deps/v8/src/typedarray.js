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



// --------------- Typed Arrays ---------------------

function CreateTypedArrayConstructor(name, elementSize, arrayId, constructor) {
  function ConstructByArrayBuffer(obj, buffer, byteOffset, length) {
    var offset = IS_UNDEFINED(byteOffset) ? 0 : TO_POSITIVE_INTEGER(byteOffset);

    if (offset % elementSize !== 0) {
      throw MakeRangeError("invalid_typed_array_alignment",
          "start offset", name, elementSize);
    }
    var bufferByteLength = %ArrayBufferGetByteLength(buffer);
    if (offset > bufferByteLength) {
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
    if (offset + newByteLength > bufferByteLength) {
      throw MakeRangeError("invalid_typed_array_length");
    }
    %TypedArrayInitialize(obj, arrayId, buffer, offset, newByteLength);
  }

  function ConstructByLength(obj, length) {
    var l = IS_UNDEFINED(length) ? 0 : TO_POSITIVE_INTEGER(length);
    var byteLength = l * elementSize;
    var buffer = new global.ArrayBuffer(byteLength);
    %TypedArrayInitialize(obj, arrayId, buffer, 0, byteLength);
  }

  function ConstructByArrayLike(obj, arrayLike) {
    var length = arrayLike.length;
    var l =  IS_UNDEFINED(length) ? 0 : TO_POSITIVE_INTEGER(length);
    var byteLength = l * elementSize;
    var buffer = new $ArrayBuffer(byteLength);
    %TypedArrayInitialize(obj, arrayId, buffer, 0, byteLength);
    for (var i = 0; i < l; i++) {
      obj[i] = arrayLike[i];
    }
  }

  return function (arg1, arg2, arg3) {
    if (%_IsConstructCall()) {
      if (IS_ARRAYBUFFER(arg1)) {
        ConstructByArrayBuffer(this, arg1, arg2, arg3);
      } else if (IS_NUMBER(arg1) || IS_STRING(arg1) || IS_BOOLEAN(arg1)) {
        ConstructByLength(this, arg1);
      } else if (!IS_UNDEFINED(arg1)){
        ConstructByArrayLike(this, arg1);
      } else {
        throw MakeTypeError("parameterless_typed_array_constr", name);
      }
    } else {
      return new constructor(arg1, arg2, arg3);
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

function CreateSubArray(elementSize, constructor) {
  return function(begin, end) {
    var srcLength = %TypedArrayGetLength(this);
    var beginInt = TO_INTEGER(begin);
    if (beginInt < 0) {
      beginInt = MathMax(0, srcLength + beginInt);
    } else {
      beginInt = MathMin(srcLength, beginInt);
    }

    var endInt = IS_UNDEFINED(end) ? srcLength : TO_INTEGER(end);
    if (endInt < 0) {
      endInt = MathMax(0, srcLength + endInt);
    } else {
      endInt = MathMin(endInt, srcLength);
    }
    if (endInt < beginInt) {
      endInt = beginInt;
    }
    var newLength = endInt - beginInt;
    var beginByteOffset =
        %TypedArrayGetByteOffset(this) + beginInt * elementSize;
    return new constructor(%TypedArrayGetBuffer(this),
                           beginByteOffset, newLength);
  }
}

function TypedArraySet(obj, offset) {
  var intOffset = IS_UNDEFINED(offset) ? 0 : TO_POSITIVE_INTEGER(offset);
  if (%TypedArraySetFastCases(this, obj, intOffset))
    return;

  var l = obj.length;
  if (IS_UNDEFINED(l)) {
    throw MakeTypeError("invalid_argument");
  }
  if (intOffset + l > this.length) {
    throw MakeRangeError("typed_array_set_source_too_large");
  }
  for (var i = 0; i < l; i++) {
    this[intOffset + i] = obj[i];
  }
}

// -------------------------------------------------------------------

function SetupTypedArray(arrayId, name, constructor, elementSize) {
  %CheckIsBootstrapping();
  var fun = CreateTypedArrayConstructor(name, elementSize,
                                        arrayId, constructor);
  %SetCode(constructor, fun);
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

  InstallFunctions(constructor.prototype, DONT_ENUM, $Array(
        "subarray", CreateSubArray(elementSize, constructor),
        "set", TypedArraySet
  ));
}

// arrayIds below should be synchronized with Runtime_TypedArrayInitialize.
SetupTypedArray(1, "Uint8Array", global.Uint8Array, 1);
SetupTypedArray(2, "Int8Array", global.Int8Array, 1);
SetupTypedArray(3, "Uint16Array", global.Uint16Array, 2);
SetupTypedArray(4, "Int16Array", global.Int16Array, 2);
SetupTypedArray(5, "Uint32Array", global.Uint32Array, 4);
SetupTypedArray(6, "Int32Array", global.Int32Array, 4);
SetupTypedArray(7, "Float32Array", global.Float32Array, 4);
SetupTypedArray(8, "Float64Array", global.Float64Array, 8);
SetupTypedArray(9, "Uint8ClampedArray", global.Uint8ClampedArray, 1);
