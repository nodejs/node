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
var $ArrayBuffer = global.ArrayBuffer;


// --------------- Typed Arrays ---------------------
macro TYPED_ARRAYS(FUNCTION)
// arrayIds below should be synchronized with Runtime_TypedArrayInitialize.
FUNCTION(1, Uint8Array, 1)
FUNCTION(2, Int8Array, 1)
FUNCTION(3, Uint16Array, 2)
FUNCTION(4, Int16Array, 2)
FUNCTION(5, Uint32Array, 4)
FUNCTION(6, Int32Array, 4)
FUNCTION(7, Float32Array, 4)
FUNCTION(8, Float64Array, 8)
FUNCTION(9, Uint8ClampedArray, 1)
endmacro

macro TYPED_ARRAY_CONSTRUCTOR(ARRAY_ID, NAME, ELEMENT_SIZE)
  function NAMEConstructByArrayBuffer(obj, buffer, byteOffset, length) {
    if (!IS_UNDEFINED(byteOffset)) {
        byteOffset =
            ToPositiveInteger(byteOffset,  "invalid_typed_array_length");
    }
    if (!IS_UNDEFINED(length)) {
        length = ToPositiveInteger(length, "invalid_typed_array_length");
    }

    var bufferByteLength = %ArrayBufferGetByteLength(buffer);
    var offset;
    if (IS_UNDEFINED(byteOffset)) {
      offset = 0;
    } else {
      offset = byteOffset;

      if (offset % ELEMENT_SIZE !== 0) {
        throw MakeRangeError("invalid_typed_array_alignment",
            ["start offset", "NAME", ELEMENT_SIZE]);
      }
      if (offset > bufferByteLength) {
        throw MakeRangeError("invalid_typed_array_offset");
      }
    }

    var newByteLength;
    var newLength;
    if (IS_UNDEFINED(length)) {
      if (bufferByteLength % ELEMENT_SIZE !== 0) {
        throw MakeRangeError("invalid_typed_array_alignment",
          ["byte length", "NAME", ELEMENT_SIZE]);
      }
      newByteLength = bufferByteLength - offset;
      newLength = newByteLength / ELEMENT_SIZE;
    } else {
      var newLength = length;
      newByteLength = newLength * ELEMENT_SIZE;
    }
    if ((offset + newByteLength > bufferByteLength)
        || (newLength > %_MaxSmi())) {
      throw MakeRangeError("invalid_typed_array_length");
    }
    %_TypedArrayInitialize(obj, ARRAY_ID, buffer, offset, newByteLength);
  }

  function NAMEConstructByLength(obj, length) {
    var l = IS_UNDEFINED(length) ?
      0 : ToPositiveInteger(length, "invalid_typed_array_length");
    if (l > %_MaxSmi()) {
      throw MakeRangeError("invalid_typed_array_length");
    }
    var byteLength = l * ELEMENT_SIZE;
    if (byteLength > %_TypedArrayMaxSizeInHeap()) {
      var buffer = new $ArrayBuffer(byteLength);
      %_TypedArrayInitialize(obj, ARRAY_ID, buffer, 0, byteLength);
    } else {
      %_TypedArrayInitialize(obj, ARRAY_ID, null, 0, byteLength);
    }
  }

  function NAMEConstructByArrayLike(obj, arrayLike) {
    var length = arrayLike.length;
    var l = ToPositiveInteger(length, "invalid_typed_array_length");

    if (l > %_MaxSmi()) {
      throw MakeRangeError("invalid_typed_array_length");
    }
    if(!%TypedArrayInitializeFromArrayLike(obj, ARRAY_ID, arrayLike, l)) {
      for (var i = 0; i < l; i++) {
        // It is crucial that we let any execptions from arrayLike[i]
        // propagate outside the function.
        obj[i] = arrayLike[i];
      }
    }
  }

  function NAMEConstructor(arg1, arg2, arg3) {

    if (%_IsConstructCall()) {
      if (IS_ARRAYBUFFER(arg1)) {
        NAMEConstructByArrayBuffer(this, arg1, arg2, arg3);
      } else if (IS_NUMBER(arg1) || IS_STRING(arg1) ||
                 IS_BOOLEAN(arg1) || IS_UNDEFINED(arg1)) {
        NAMEConstructByLength(this, arg1);
      } else {
        NAMEConstructByArrayLike(this, arg1);
      }
    } else {
      throw MakeTypeError("constructor_not_function", ["NAME"])
    }
  }
endmacro

TYPED_ARRAYS(TYPED_ARRAY_CONSTRUCTOR)

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
    var beginInt = TO_INTEGER(begin);
    if (!IS_UNDEFINED(end)) {
      end = TO_INTEGER(end);
    }

    var srcLength = %TypedArrayGetLength(this);
    if (beginInt < 0) {
      beginInt = MathMax(0, srcLength + beginInt);
    } else {
      beginInt = MathMin(srcLength, beginInt);
    }

    var endInt = IS_UNDEFINED(end) ? srcLength : end;
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

function TypedArraySetFromArrayLike(target, source, sourceLength, offset) {
  if (offset > 0) {
    for (var i = 0; i < sourceLength; i++) {
      target[offset + i] = source[i];
    }
  }
  else {
    for (var i = 0; i < sourceLength; i++) {
      target[i] = source[i];
    }
  }
}

function TypedArraySetFromOverlappingTypedArray(target, source, offset) {
  var sourceElementSize = source.BYTES_PER_ELEMENT;
  var targetElementSize = target.BYTES_PER_ELEMENT;
  var sourceLength = source.length;

  // Copy left part.
  function CopyLeftPart() {
    // First un-mutated byte after the next write
    var targetPtr = target.byteOffset + (offset + 1) * targetElementSize;
    // Next read at sourcePtr. We do not care for memory changing before
    // sourcePtr - we have already copied it.
    var sourcePtr = source.byteOffset;
    for (var leftIndex = 0;
         leftIndex < sourceLength && targetPtr <= sourcePtr;
         leftIndex++) {
      target[offset + leftIndex] = source[leftIndex];
      targetPtr += targetElementSize;
      sourcePtr += sourceElementSize;
    }
    return leftIndex;
  }
  var leftIndex = CopyLeftPart();

  // Copy rigth part;
  function CopyRightPart() {
    // First unmutated byte before the next write
    var targetPtr =
      target.byteOffset + (offset + sourceLength - 1) * targetElementSize;
    // Next read before sourcePtr. We do not care for memory changing after
    // sourcePtr - we have already copied it.
    var sourcePtr =
      source.byteOffset + sourceLength * sourceElementSize;
    for(var rightIndex = sourceLength - 1;
        rightIndex >= leftIndex && targetPtr >= sourcePtr;
        rightIndex--) {
      target[offset + rightIndex] = source[rightIndex];
      targetPtr -= targetElementSize;
      sourcePtr -= sourceElementSize;
    }
    return rightIndex;
  }
  var rightIndex = CopyRightPart();

  var temp = new $Array(rightIndex + 1 - leftIndex);
  for (var i = leftIndex; i <= rightIndex; i++) {
    temp[i - leftIndex] = source[i];
  }
  for (i = leftIndex; i <= rightIndex; i++) {
    target[offset + i] = temp[i - leftIndex];
  }
}

function TypedArraySet(obj, offset) {
  var intOffset = IS_UNDEFINED(offset) ? 0 : TO_INTEGER(offset);
  if (intOffset < 0) {
    throw MakeTypeError("typed_array_set_negative_offset");
  }

  if (intOffset > %_MaxSmi()) {
    throw MakeRangeError("typed_array_set_source_too_large");
  }
  switch (%TypedArraySetFastCases(this, obj, intOffset)) {
    // These numbers should be synchronized with runtime.cc.
    case 0: // TYPED_ARRAY_SET_TYPED_ARRAY_SAME_TYPE
      return;
    case 1: // TYPED_ARRAY_SET_TYPED_ARRAY_OVERLAPPING
      TypedArraySetFromOverlappingTypedArray(this, obj, intOffset);
      return;
    case 2: // TYPED_ARRAY_SET_TYPED_ARRAY_NONOVERLAPPING
      TypedArraySetFromArrayLike(this, obj, obj.length, intOffset);
      return;
    case 3: // TYPED_ARRAY_SET_NON_TYPED_ARRAY
      var l = obj.length;
      if (IS_UNDEFINED(l)) {
        if (IS_NUMBER(obj)) {
            // For number as a first argument, throw TypeError
            // instead of silently ignoring the call, so that
            // the user knows (s)he did something wrong.
            // (Consistent with Firefox and Blink/WebKit)
            throw MakeTypeError("invalid_argument");
        }
        return;
      }
      if (intOffset + l > this.length) {
        throw MakeRangeError("typed_array_set_source_too_large");
      }
      TypedArraySetFromArrayLike(this, obj, l, intOffset);
      return;
  }
}

// -------------------------------------------------------------------

function SetupTypedArray(constructor, fun, elementSize) {
  %CheckIsBootstrapping();
  %SetCode(constructor, fun);
  %FunctionSetPrototype(constructor, new $Object());

  %SetProperty(constructor, "BYTES_PER_ELEMENT", elementSize,
               READ_ONLY | DONT_ENUM | DONT_DELETE);
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

macro SETUP_TYPED_ARRAY(ARRAY_ID, NAME, ELEMENT_SIZE)
  SetupTypedArray (global.NAME, NAMEConstructor, ELEMENT_SIZE);
endmacro

TYPED_ARRAYS(SETUP_TYPED_ARRAY)

// --------------------------- DataView -----------------------------

var $DataView = global.DataView;

function DataViewConstructor(buffer, byteOffset, byteLength) { // length = 3
  if (%_IsConstructCall()) {
    if (!IS_ARRAYBUFFER(buffer)) {
      throw MakeTypeError('data_view_not_array_buffer', []);
    }
    if (!IS_UNDEFINED(byteOffset)) {
        byteOffset = ToPositiveInteger(byteOffset, 'invalid_data_view_offset');
    }
    if (!IS_UNDEFINED(byteLength)) {
        byteLength = TO_INTEGER(byteLength);
    }

    var bufferByteLength = %ArrayBufferGetByteLength(buffer);

    var offset = IS_UNDEFINED(byteOffset) ?  0 : byteOffset;
    if (offset > bufferByteLength) {
      throw MakeRangeError('invalid_data_view_offset');
    }

    var length = IS_UNDEFINED(byteLength)
        ? bufferByteLength - offset
        : byteLength;
    if (length < 0 || offset + length > bufferByteLength) {
      throw new MakeRangeError('invalid_data_view_length');
    }
    %_DataViewInitialize(this, buffer, offset, length);
  } else {
    throw MakeTypeError('constructor_not_function', ["DataView"]);
  }
}

function DataViewGetBuffer() {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.buffer', this]);
  }
  return %DataViewGetBuffer(this);
}

function DataViewGetByteOffset() {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.byteOffset', this]);
  }
  return %DataViewGetByteOffset(this);
}

function DataViewGetByteLength() {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.byteLength', this]);
  }
  return %DataViewGetByteLength(this);
}

macro DATA_VIEW_TYPES(FUNCTION)
  FUNCTION(Int8)
  FUNCTION(Uint8)
  FUNCTION(Int16)
  FUNCTION(Uint16)
  FUNCTION(Int32)
  FUNCTION(Uint32)
  FUNCTION(Float32)
  FUNCTION(Float64)
endmacro

function ToPositiveDataViewOffset(offset) {
  return ToPositiveInteger(offset, 'invalid_data_view_accessor_offset');
}


macro DATA_VIEW_GETTER_SETTER(TYPENAME)
function DataViewGetTYPENAME(offset, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.getTYPENAME', this]);
  }
  if (%_ArgumentsLength() < 1) {
    throw MakeTypeError('invalid_argument');
  }
  return %DataViewGetTYPENAME(this,
                          ToPositiveDataViewOffset(offset),
                          !!little_endian);
}

function DataViewSetTYPENAME(offset, value, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.setTYPENAME', this]);
  }
  if (%_ArgumentsLength() < 2) {
    throw MakeTypeError('invalid_argument');
  }
  %DataViewSetTYPENAME(this,
                   ToPositiveDataViewOffset(offset),
                   TO_NUMBER_INLINE(value),
                   !!little_endian);
}
endmacro

DATA_VIEW_TYPES(DATA_VIEW_GETTER_SETTER)

function SetupDataView() {
  %CheckIsBootstrapping();

  // Setup the DataView constructor.
  %SetCode($DataView, DataViewConstructor);
  %FunctionSetPrototype($DataView, new $Object);

  // Set up constructor property on the DataView prototype.
  %SetProperty($DataView.prototype, "constructor", $DataView, DONT_ENUM);

  InstallGetter($DataView.prototype, "buffer", DataViewGetBuffer);
  InstallGetter($DataView.prototype, "byteOffset", DataViewGetByteOffset);
  InstallGetter($DataView.prototype, "byteLength", DataViewGetByteLength);

  InstallFunctions($DataView.prototype, DONT_ENUM, $Array(
      "getInt8", DataViewGetInt8,
      "setInt8", DataViewSetInt8,

      "getUint8", DataViewGetUint8,
      "setUint8", DataViewSetUint8,

      "getInt16", DataViewGetInt16,
      "setInt16", DataViewSetInt16,

      "getUint16", DataViewGetUint16,
      "setUint16", DataViewSetUint16,

      "getInt32", DataViewGetInt32,
      "setInt32", DataViewSetInt32,

      "getUint32", DataViewGetUint32,
      "setUint32", DataViewSetUint32,

      "getFloat32", DataViewGetFloat32,
      "setFloat32", DataViewSetFloat32,

      "getFloat64", DataViewGetFloat64,
      "setFloat64", DataViewSetFloat64
  ));
}

SetupDataView();
