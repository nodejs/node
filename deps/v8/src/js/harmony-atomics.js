// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalObject = global.Object;
var MakeTypeError;
var MaxSimple;
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
  MaxSimple = from.MaxSimple;
});

// -------------------------------------------------------------------


function CheckSharedIntegerTypedArray(ia) {
  if (!%IsSharedIntegerTypedArray(ia)) {
    throw MakeTypeError(kNotIntegerSharedTypedArray, ia);
  }
}

function CheckSharedInteger32TypedArray(ia) {
  CheckSharedIntegerTypedArray(ia);
  if (!%IsSharedInteger32TypedArray(ia)) {
    throw MakeTypeError(kNotInt32SharedTypedArray, ia);
  }
}

//-------------------------------------------------------------------

function AtomicsCompareExchangeJS(sta, index, oldValue, newValue) {
  CheckSharedIntegerTypedArray(sta);
  index = TO_INTEGER(index);
  if (index < 0 || index >= %_TypedArrayGetLength(sta)) {
    return UNDEFINED;
  }
  oldValue = TO_NUMBER(oldValue);
  newValue = TO_NUMBER(newValue);
  return %_AtomicsCompareExchange(sta, index, oldValue, newValue);
}

function AtomicsLoadJS(sta, index) {
  CheckSharedIntegerTypedArray(sta);
  index = TO_INTEGER(index);
  if (index < 0 || index >= %_TypedArrayGetLength(sta)) {
    return UNDEFINED;
  }
  return %_AtomicsLoad(sta, index);
}

function AtomicsStoreJS(sta, index, value) {
  CheckSharedIntegerTypedArray(sta);
  index = TO_INTEGER(index);
  if (index < 0 || index >= %_TypedArrayGetLength(sta)) {
    return UNDEFINED;
  }
  value = TO_NUMBER(value);
  return %_AtomicsStore(sta, index, value);
}

function AtomicsAddJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = TO_INTEGER(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = TO_NUMBER(value);
  return %_AtomicsAdd(ia, index, value);
}

function AtomicsSubJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = TO_INTEGER(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = TO_NUMBER(value);
  return %_AtomicsSub(ia, index, value);
}

function AtomicsAndJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = TO_INTEGER(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = TO_NUMBER(value);
  return %_AtomicsAnd(ia, index, value);
}

function AtomicsOrJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = TO_INTEGER(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = TO_NUMBER(value);
  return %_AtomicsOr(ia, index, value);
}

function AtomicsXorJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = TO_INTEGER(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = TO_NUMBER(value);
  return %_AtomicsXor(ia, index, value);
}

function AtomicsExchangeJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = TO_INTEGER(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = TO_NUMBER(value);
  return %_AtomicsExchange(ia, index, value);
}

function AtomicsIsLockFreeJS(size) {
  return %_AtomicsIsLockFree(size);
}

// Futexes

function AtomicsFutexWaitJS(ia, index, value, timeout) {
  CheckSharedInteger32TypedArray(ia);
  index = TO_INTEGER(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
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
  return %AtomicsFutexWait(ia, index, value, timeout);
}

function AtomicsFutexWakeJS(ia, index, count) {
  CheckSharedInteger32TypedArray(ia);
  index = TO_INTEGER(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  count = MaxSimple(0, TO_INTEGER(count));
  return %AtomicsFutexWake(ia, index, count);
}

function AtomicsFutexWakeOrRequeueJS(ia, index1, count, value, index2) {
  CheckSharedInteger32TypedArray(ia);
  index1 = TO_INTEGER(index1);
  count = MaxSimple(0, TO_INTEGER(count));
  value = TO_INT32(value);
  index2 = TO_INTEGER(index2);
  if (index1 < 0 || index1 >= %_TypedArrayGetLength(ia) ||
      index2 < 0 || index2 >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  return %AtomicsFutexWakeOrRequeue(ia, index1, count, value, index2);
}

// -------------------------------------------------------------------

function AtomicsConstructor() {}

var Atomics = new AtomicsConstructor();

%InternalSetPrototype(Atomics, GlobalObject.prototype);
%AddNamedProperty(global, "Atomics", Atomics, DONT_ENUM);
%FunctionSetInstanceClassName(AtomicsConstructor, 'Atomics');

%AddNamedProperty(Atomics, toStringTagSymbol, "Atomics", READ_ONLY | DONT_ENUM);

// These must match the values in src/futex-emulation.h
utils.InstallConstants(Atomics, [
  "OK", 0,
  "NOTEQUAL", -1,
  "TIMEDOUT", -2,
]);

utils.InstallFunctions(Atomics, DONT_ENUM, [
  "compareExchange", AtomicsCompareExchangeJS,
  "load", AtomicsLoadJS,
  "store", AtomicsStoreJS,
  "add", AtomicsAddJS,
  "sub", AtomicsSubJS,
  "and", AtomicsAndJS,
  "or", AtomicsOrJS,
  "xor", AtomicsXorJS,
  "exchange", AtomicsExchangeJS,
  "isLockFree", AtomicsIsLockFreeJS,
  "futexWait", AtomicsFutexWaitJS,
  "futexWake", AtomicsFutexWakeJS,
  "futexWakeOrRequeue", AtomicsFutexWakeOrRequeueJS,
]);

})
