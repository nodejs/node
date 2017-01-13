// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalObject = global.Object;
var MaxSimple;
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

utils.Import(function(from) {
  MaxSimple = from.MaxSimple;
});

// -------------------------------------------------------------------


function CheckSharedIntegerTypedArray(ia) {
  if (!%IsSharedIntegerTypedArray(ia)) {
    throw %make_type_error(kNotIntegerSharedTypedArray, ia);
  }
}

function CheckSharedInteger32TypedArray(ia) {
  CheckSharedIntegerTypedArray(ia);
  if (!%IsSharedInteger32TypedArray(ia)) {
    throw %make_type_error(kNotInt32SharedTypedArray, ia);
  }
}

// https://tc39.github.io/ecmascript_sharedmem/shmem.html#Atomics.ValidateAtomicAccess
function ValidateIndex(index, length) {
  var numberIndex = TO_NUMBER(index);
  var accessIndex = TO_INTEGER(numberIndex);
  if (numberIndex !== accessIndex) {
    throw %make_range_error(kInvalidAtomicAccessIndex);
  }
  if (accessIndex < 0 || accessIndex >= length) {
    throw %make_range_error(kInvalidAtomicAccessIndex);
  }
  return accessIndex;
}

//-------------------------------------------------------------------

function AtomicsCompareExchangeJS(sta, index, oldValue, newValue) {
  CheckSharedIntegerTypedArray(sta);
  index = ValidateIndex(index, %_TypedArrayGetLength(sta));
  oldValue = TO_NUMBER(oldValue);
  newValue = TO_NUMBER(newValue);
  return %_AtomicsCompareExchange(sta, index, oldValue, newValue);
}

function AtomicsAddJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = ValidateIndex(index, %_TypedArrayGetLength(ia));
  value = TO_NUMBER(value);
  return %_AtomicsAdd(ia, index, value);
}

function AtomicsSubJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = ValidateIndex(index, %_TypedArrayGetLength(ia));
  value = TO_NUMBER(value);
  return %_AtomicsSub(ia, index, value);
}

function AtomicsAndJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = ValidateIndex(index, %_TypedArrayGetLength(ia));
  value = TO_NUMBER(value);
  return %_AtomicsAnd(ia, index, value);
}

function AtomicsOrJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = ValidateIndex(index, %_TypedArrayGetLength(ia));
  value = TO_NUMBER(value);
  return %_AtomicsOr(ia, index, value);
}

function AtomicsXorJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = ValidateIndex(index, %_TypedArrayGetLength(ia));
  value = TO_NUMBER(value);
  return %_AtomicsXor(ia, index, value);
}

function AtomicsExchangeJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = ValidateIndex(index, %_TypedArrayGetLength(ia));
  value = TO_NUMBER(value);
  return %_AtomicsExchange(ia, index, value);
}

function AtomicsIsLockFreeJS(size) {
  return %_AtomicsIsLockFree(size);
}

function AtomicsWaitJS(ia, index, value, timeout) {
  CheckSharedInteger32TypedArray(ia);
  index = ValidateIndex(index, %_TypedArrayGetLength(ia));
  if (IS_UNDEFINED(timeout)) {
    timeout = INFINITY;
  } else {
    timeout = TO_NUMBER(timeout);
    if (NUMBER_IS_NAN(timeout)) {
      timeout = INFINITY;
    } else {
      timeout = MaxSimple(0, timeout);
    }
  }
  return %AtomicsWait(ia, index, value, timeout);
}

function AtomicsWakeJS(ia, index, count) {
  CheckSharedInteger32TypedArray(ia);
  index = ValidateIndex(index, %_TypedArrayGetLength(ia));
  count = MaxSimple(0, TO_INTEGER(count));
  return %AtomicsWake(ia, index, count);
}

// -------------------------------------------------------------------

var Atomics = global.Atomics;

// The Atomics global is defined by the bootstrapper.

%AddNamedProperty(Atomics, toStringTagSymbol, "Atomics", READ_ONLY | DONT_ENUM);

utils.InstallFunctions(Atomics, DONT_ENUM, [
  // TODO(binji): remove the rest of the (non futex) Atomics functions as they
  // become builtins.
  "compareExchange", AtomicsCompareExchangeJS,
  "add", AtomicsAddJS,
  "sub", AtomicsSubJS,
  "and", AtomicsAndJS,
  "or", AtomicsOrJS,
  "xor", AtomicsXorJS,
  "exchange", AtomicsExchangeJS,
  "isLockFree", AtomicsIsLockFreeJS,
  "wait", AtomicsWaitJS,
  "wake", AtomicsWakeJS,
]);

})
