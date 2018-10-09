// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

// array.js has to come before typedarray.js for this to work
var ArrayToString = utils.ImportNow("ArrayToString");
var InnerArrayJoin;
var InnerArrayToLocaleString;

macro TYPED_ARRAYS(FUNCTION)
FUNCTION(Uint8Array, 1)
FUNCTION(Int8Array, 1)
FUNCTION(Uint16Array, 2)
FUNCTION(Int16Array, 2)
FUNCTION(Uint32Array, 4)
FUNCTION(Int32Array, 4)
FUNCTION(Float32Array, 4)
FUNCTION(Float64Array, 8)
FUNCTION(Uint8ClampedArray, 1)
FUNCTION(BigUint64Array, 8)
FUNCTION(BigInt64Array, 8)
endmacro

macro DECLARE_GLOBALS(NAME, SIZE)
var GlobalNAME = global.NAME;
endmacro

TYPED_ARRAYS(DECLARE_GLOBALS)

macro IS_TYPEDARRAY(arg)
(%_IsTypedArray(arg))
endmacro

var GlobalTypedArray = %object_get_prototype_of(GlobalUint8Array);

utils.Import(function(from) {
  InnerArrayJoin = from.InnerArrayJoin;
  InnerArrayToLocaleString = from.InnerArrayToLocaleString;
});

// --------------- Typed Arrays ---------------------

// ES6 section 22.2.3.5.1 ValidateTypedArray ( O )
function ValidateTypedArray(array, methodName) {
  if (!IS_TYPEDARRAY(array)) throw %make_type_error(kNotTypedArray);

  if (%ArrayBufferViewWasNeutered(array))
    throw %make_type_error(kDetachedOperation, methodName);
}


// ES6 section 22.2.3.27
// ecma402 #sup-array.prototype.tolocalestring
DEFINE_METHOD(
  GlobalTypedArray.prototype,
  toLocaleString() {
    ValidateTypedArray(this, "%TypedArray%.prototype.toLocaleString");

    var locales = arguments[0];
    var options = arguments[1];
    var length = %TypedArrayGetLength(this);
    return InnerArrayToLocaleString(this, length, locales, options);
  }
);


// ES6 section 22.2.3.14
DEFINE_METHOD(
  GlobalTypedArray.prototype,
  join(separator) {
    ValidateTypedArray(this, "%TypedArray%.prototype.join");

    var length = %TypedArrayGetLength(this);

    return InnerArrayJoin(separator, this, length);
  }
);

// -------------------------------------------------------------------

%AddNamedProperty(GlobalTypedArray.prototype, "toString", ArrayToString,
                  DONT_ENUM);

})
