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

function CreateTypedArrayConstructor(name, elementSize, arrayId, constructor) {
  function ConstructByArrayBuffer(obj, buffer, byteOffset, length) {
    var offset = ToPositiveInteger(byteOffset, "invalid_typed_array_length")

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
      var newLength = ToPositiveInteger(length, "invalid_typed_array_length");
      newByteLength = newLength * elementSize;
    }
    if (offset + newByteLength > bufferByteLength) {
      throw MakeRangeError("invalid_typed_array_length");
    }
    %TypedArrayInitialize(obj, arrayId, buffer, offset, newByteLength);
  }

  function ConstructByLength(obj, length) {
    var l = ToPositiveInteger(length, "invalid_typed_array_length");
    if (l > %MaxSmi()) {
      throw MakeRangeError("invalid_typed_array_length");
    }
    var byteLength = l * elementSize;
    var buffer = new $ArrayBuffer(byteLength);
    %TypedArrayInitialize(obj, arrayId, buffer, 0, byteLength);
  }

  function ConstructByArrayLike(obj, arrayLike) {
    var length = arrayLike.length;
    var l = ToPositiveInteger(length, "invalid_typed_array_length");
    if(!%TypedArrayInitializeFromArrayLike(obj, arrayId, arrayLike, l)) {
      for (var i = 0; i < l; i++) {
        // It is crucial that we let any execptions from arrayLike[i]
        // propagate outside the function.
        obj[i] = arrayLike[i];
      }
    }
  }

  return function (arg1, arg2, arg3) {
    if (%_IsConstructCall()) {
      if (IS_ARRAYBUFFER(arg1)) {
        ConstructByArrayBuffer(this, arg1, arg2, arg3);
      } else if (IS_NUMBER(arg1) || IS_STRING(arg1) ||
                 IS_BOOLEAN(arg1) || IS_UNDEFINED(arg1)) {
        ConstructByLength(this, arg1);
      } else {
        ConstructByArrayLike(this, arg1);
      }
    } else {
      throw MakeTypeError("constructor_not_function", [name])
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

function SetupTypedArray(arrayId, name, constructor, elementSize) {
  %CheckIsBootstrapping();
  var fun = CreateTypedArrayConstructor(name, elementSize,
                                        arrayId, constructor);
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


// --------------------------- DataView -----------------------------

var $DataView = global.DataView;

function DataViewConstructor(buffer, byteOffset, byteLength) { // length = 3
  if (%_IsConstructCall()) {
    if (!IS_ARRAYBUFFER(buffer)) {
      throw MakeTypeError('data_view_not_array_buffer', []);
    }
    var bufferByteLength = %ArrayBufferGetByteLength(buffer);
    var offset = ToPositiveInteger(byteOffset, 'invalid_data_view_offset');
    if (offset > bufferByteLength) {
      throw MakeRangeError('invalid_data_view_offset');
    }
    var length = IS_UNDEFINED(byteLength) ?
      bufferByteLength - offset : TO_INTEGER(byteLength);
    if (length < 0 || offset + length > bufferByteLength) {
      throw new MakeRangeError('invalid_data_view_length');
    }
    %DataViewInitialize(this, buffer, offset, length);
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

function ToPositiveDataViewOffset(offset) {
  return ToPositiveInteger(offset, 'invalid_data_view_accessor_offset');
}

function DataViewGetInt8(offset, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.getInt8', this]);
  }
  if (%_ArgumentsLength() < 1) {
    throw MakeTypeError('invalid_argument');
  }
  return %DataViewGetInt8(this,
                          ToPositiveDataViewOffset(offset),
                          !!little_endian);
}

function DataViewSetInt8(offset, value, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.setInt8', this]);
  }
  if (%_ArgumentsLength() < 2) {
    throw MakeTypeError('invalid_argument');
  }
  %DataViewSetInt8(this,
                   ToPositiveDataViewOffset(offset),
                   TO_NUMBER_INLINE(value),
                   !!little_endian);
}

function DataViewGetUint8(offset, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.getUint8', this]);
  }
  if (%_ArgumentsLength() < 1) {
    throw MakeTypeError('invalid_argument');
  }
  return %DataViewGetUint8(this,
                           ToPositiveDataViewOffset(offset),
                           !!little_endian);
}

function DataViewSetUint8(offset, value, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.setUint8', this]);
  }
  if (%_ArgumentsLength() < 2) {
    throw MakeTypeError('invalid_argument');
  }
  %DataViewSetUint8(this,
                   ToPositiveDataViewOffset(offset),
                   TO_NUMBER_INLINE(value),
                   !!little_endian);
}

function DataViewGetInt16(offset, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.getInt16', this]);
  }
  if (%_ArgumentsLength() < 1) {
    throw MakeTypeError('invalid_argument');
  }
  return %DataViewGetInt16(this,
                           ToPositiveDataViewOffset(offset),
                           !!little_endian);
}

function DataViewSetInt16(offset, value, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.setInt16', this]);
  }
  if (%_ArgumentsLength() < 2) {
    throw MakeTypeError('invalid_argument');
  }
  %DataViewSetInt16(this,
                    ToPositiveDataViewOffset(offset),
                    TO_NUMBER_INLINE(value),
                    !!little_endian);
}

function DataViewGetUint16(offset, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.getUint16', this]);
  }
  if (%_ArgumentsLength() < 1) {
    throw MakeTypeError('invalid_argument');
  }
  return %DataViewGetUint16(this,
                            ToPositiveDataViewOffset(offset),
                            !!little_endian);
}

function DataViewSetUint16(offset, value, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.setUint16', this]);
  }
  if (%_ArgumentsLength() < 2) {
    throw MakeTypeError('invalid_argument');
  }
  %DataViewSetUint16(this,
                     ToPositiveDataViewOffset(offset),
                     TO_NUMBER_INLINE(value),
                     !!little_endian);
}

function DataViewGetInt32(offset, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.getInt32', this]);
  }
  if (%_ArgumentsLength() < 1) {
    throw MakeTypeError('invalid_argument');
  }
  return %DataViewGetInt32(this,
                           ToPositiveDataViewOffset(offset),
                           !!little_endian);
}

function DataViewSetInt32(offset, value, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.setInt32', this]);
  }
  if (%_ArgumentsLength() < 2) {
    throw MakeTypeError('invalid_argument');
  }
  %DataViewSetInt32(this,
                    ToPositiveDataViewOffset(offset),
                    TO_NUMBER_INLINE(value),
                    !!little_endian);
}

function DataViewGetUint32(offset, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.getUint32', this]);
  }
  if (%_ArgumentsLength() < 1) {
    throw MakeTypeError('invalid_argument');
  }
  return %DataViewGetUint32(this,
                            ToPositiveDataViewOffset(offset),
                            !!little_endian);
}

function DataViewSetUint32(offset, value, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.setUint32', this]);
  }
  if (%_ArgumentsLength() < 2) {
    throw MakeTypeError('invalid_argument');
  }
  %DataViewSetUint32(this,
                     ToPositiveDataViewOffset(offset),
                     TO_NUMBER_INLINE(value),
                     !!little_endian);
}

function DataViewGetFloat32(offset, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.getFloat32', this]);
  }
  if (%_ArgumentsLength() < 1) {
    throw MakeTypeError('invalid_argument');
  }
  return %DataViewGetFloat32(this,
                             ToPositiveDataViewOffset(offset),
                             !!little_endian);
}

function DataViewSetFloat32(offset, value, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.setFloat32', this]);
  }
  if (%_ArgumentsLength() < 2) {
    throw MakeTypeError('invalid_argument');
  }
  %DataViewSetFloat32(this,
                      ToPositiveDataViewOffset(offset),
                      TO_NUMBER_INLINE(value),
                      !!little_endian);
}

function DataViewGetFloat64(offset, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.getFloat64', this]);
  }
  if (%_ArgumentsLength() < 1) {
    throw MakeTypeError('invalid_argument');
  }
  return %DataViewGetFloat64(this,
                             ToPositiveDataViewOffset(offset),
                             !!little_endian);
}

function DataViewSetFloat64(offset, value, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['DataView.setFloat64', this]);
  }
  if (%_ArgumentsLength() < 2) {
    throw MakeTypeError('invalid_argument');
  }
  %DataViewSetFloat64(this,
                      ToPositiveDataViewOffset(offset),
                      TO_NUMBER_INLINE(value),
                      !!little_endian);
}

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
