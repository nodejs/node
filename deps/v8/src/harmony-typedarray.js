// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, shared, exports) {

"use strict";

%CheckIsBootstrapping();

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

TYPED_ARRAYS(DECLARE_GLOBALS)

// -------------------------------------------------------------------

// ES6 draft 05-05-15, section 22.2.3.7
function TypedArrayEvery(f /* thisArg */) {  // length == 1
  if (!%IsTypedArray(this)) {
    throw MakeTypeError('not_typed_array', []);
  }
  if (!IS_SPEC_FUNCTION(f)) throw MakeTypeError(kCalledNonCallable, f);

  var length = %_TypedArrayGetLength(this);
  var receiver;

  if (%_ArgumentsLength() > 1) {
    receiver = %_Arguments(1);
  }

  var needs_wrapper = false;
  if (IS_NULL(receiver)) {
    if (%IsSloppyModeFunction(mapfn)) receiver = UNDEFINED;
  } else if (!IS_UNDEFINED(receiver)) {
    needs_wrapper = SHOULD_CREATE_WRAPPER(f, receiver);
  }

  var stepping = DEBUG_IS_ACTIVE && %DebugCallbackSupportsStepping(f);
  for (var i = 0; i < length; i++) {
    var element = this[i];
    // Prepare break slots for debugger step in.
    if (stepping) %DebugPrepareStepInIfStepping(f);
    var new_receiver = needs_wrapper ? $toObject(receiver) : receiver;
    if (!%_CallFunction(new_receiver, TO_OBJECT_INLINE(element), i, this, f)) {
      return false;
    }
  }
  return true;
}

// ES6 draft 08-24-14, section 22.2.3.12
function TypedArrayForEach(f /* thisArg */) {  // length == 1
  if (!%IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);
  if (!IS_SPEC_FUNCTION(f)) throw MakeTypeError(kCalledNonCallable, f);

  var length = %_TypedArrayGetLength(this);
  var receiver;

  if (%_ArgumentsLength() > 1) {
    receiver = %_Arguments(1);
  }

  var needs_wrapper = false;
  if (IS_NULL(receiver)) {
    if (%IsSloppyModeFunction(mapfn)) receiver = UNDEFINED;
  } else if (!IS_UNDEFINED(receiver)) {
    needs_wrapper = SHOULD_CREATE_WRAPPER(f, receiver);
  }

  var stepping = DEBUG_IS_ACTIVE && %DebugCallbackSupportsStepping(f);
  for (var i = 0; i < length; i++) {
    var element = this[i];
    // Prepare break slots for debugger step in.
    if (stepping) %DebugPrepareStepInIfStepping(f);
    var new_receiver = needs_wrapper ? $toObject(receiver) : receiver;
    %_CallFunction(new_receiver, TO_OBJECT_INLINE(element), i, this, f);
  }
}

// ES6 draft 08-24-14, section 22.2.2.2
function TypedArrayOf() {  // length == 0
  var length = %_ArgumentsLength();
  var array = new this(length);
  for (var i = 0; i < length; i++) {
    array[i] = %_Arguments(i);
  }
  return array;
}

macro EXTEND_TYPED_ARRAY(NAME)
  // Set up non-enumerable functions on the object.
  $installFunctions(GlobalNAME, DONT_ENUM | DONT_DELETE | READ_ONLY, [
    "of", TypedArrayOf
  ]);

  // Set up non-enumerable functions on the prototype object.
  $installFunctions(GlobalNAME.prototype, DONT_ENUM, [
    "every", TypedArrayEvery,
    "forEach", TypedArrayForEach
  ]);
endmacro

TYPED_ARRAYS(EXTEND_TYPED_ARRAY)

})
