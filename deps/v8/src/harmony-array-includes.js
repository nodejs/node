// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

'use strict';

%CheckIsBootstrapping();

var GlobalArray = global.Array;

// -------------------------------------------------------------------

// Proposed for ES7
// https://github.com/tc39/Array.prototype.includes
// 46c7532ec8499dea3e51aeb940d09e07547ed3f5
function InnerArrayIncludes(searchElement, fromIndex, array, length) {
  if (length === 0) {
    return false;
  }

  var n = $toInteger(fromIndex);

  var k;
  if (n >= 0) {
    k = n;
  } else {
    k = length + n;
    if (k < 0) {
      k = 0;
    }
  }

  while (k < length) {
    var elementK = array[k];
    if ($sameValueZero(searchElement, elementK)) {
      return true;
    }

    ++k;
  }

  return false;
}


function ArrayIncludes(searchElement, fromIndex) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.includes");

  var array = TO_OBJECT(this);
  var length = $toLength(array.length);

  return InnerArrayIncludes(searchElement, fromIndex, array, length);
}


function TypedArrayIncludes(searchElement, fromIndex) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayIncludes(searchElement, fromIndex, this, length);
}

// -------------------------------------------------------------------

%FunctionSetLength(ArrayIncludes, 1);
%FunctionSetLength(TypedArrayIncludes, 1);

// Set up the non-enumerable function on the Array prototype object.
utils.InstallFunctions(GlobalArray.prototype, DONT_ENUM, [
  "includes", ArrayIncludes
]);

// Set up the non-enumerable function on the typed array prototypes.
// This duplicates some of the machinery in harmony-typedarray.js in order to
// keep includes behind the separate --harmony-array-includes flag.
// TODO(littledan): Fix the TypedArray proto chain (bug v8:4085).

macro TYPED_ARRAYS(FUNCTION)
// arrayIds below should be synchronized with Runtime_TypedArrayInitialize.
FUNCTION(Uint8Array)
FUNCTION(Int8Array)
FUNCTION(Uint16Array)
FUNCTION(Int16Array)
FUNCTION(Uint32Array)
FUNCTION(Int32Array)
FUNCTION(Float32Array)
FUNCTION(Float64Array)
FUNCTION(Uint8ClampedArray)
endmacro

macro DECLARE_GLOBALS(NAME)
var GlobalNAME = global.NAME;
endmacro

macro EXTEND_TYPED_ARRAY(NAME)
// Set up non-enumerable functions on the prototype object.
utils.InstallFunctions(GlobalNAME.prototype, DONT_ENUM, [
  "includes", TypedArrayIncludes
]);
endmacro

TYPED_ARRAYS(DECLARE_GLOBALS)
TYPED_ARRAYS(EXTEND_TYPED_ARRAY)

})
