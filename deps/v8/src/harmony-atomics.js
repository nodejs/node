// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalObject = global.Object;
var MathMax;
var ToNumber;

utils.Import(function(from) {
  MathMax = from.MathMax;
  ToNumber = from.ToNumber;
});

// -------------------------------------------------------------------


function CheckSharedTypedArray(sta) {
  if (!%IsSharedTypedArray(sta)) {
    throw MakeTypeError(kNotSharedTypedArray, sta);
  }
}

function CheckSharedIntegerTypedArray(ia) {
  if (!%IsSharedIntegerTypedArray(ia)) {
    throw MakeTypeError(kNotIntegerSharedTypedArray, ia);
  }
}

function CheckSharedInteger32TypedArray(ia) {
  CheckSharedIntegerTypedArray(ia);
  if (%_ClassOf(ia) !== 'Int32Array') {
    throw MakeTypeError(kNotInt32SharedTypedArray, ia);
  }
}

//-------------------------------------------------------------------

function AtomicsCompareExchangeJS(sta, index, oldValue, newValue) {
  CheckSharedTypedArray(sta);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(sta)) {
    return UNDEFINED;
  }
  oldValue = ToNumber(oldValue);
  newValue = ToNumber(newValue);
  return %_AtomicsCompareExchange(sta, index, oldValue, newValue);
}

function AtomicsLoadJS(sta, index) {
  CheckSharedTypedArray(sta);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(sta)) {
    return UNDEFINED;
  }
  return %_AtomicsLoad(sta, index);
}

function AtomicsStoreJS(sta, index, value) {
  CheckSharedTypedArray(sta);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(sta)) {
    return UNDEFINED;
  }
  value = ToNumber(value);
  return %_AtomicsStore(sta, index, value);
}

function AtomicsAddJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = ToNumber(value);
  return %_AtomicsAdd(ia, index, value);
}

function AtomicsSubJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = ToNumber(value);
  return %_AtomicsSub(ia, index, value);
}

function AtomicsAndJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = ToNumber(value);
  return %_AtomicsAnd(ia, index, value);
}

function AtomicsOrJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = ToNumber(value);
  return %_AtomicsOr(ia, index, value);
}

function AtomicsXorJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = ToNumber(value);
  return %_AtomicsXor(ia, index, value);
}

function AtomicsExchangeJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = ToNumber(value);
  return %_AtomicsExchange(ia, index, value);
}

function AtomicsIsLockFreeJS(size) {
  return %_AtomicsIsLockFree(size);
}

// Futexes

function AtomicsFutexWaitJS(ia, index, value, timeout) {
  CheckSharedInteger32TypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  if (IS_UNDEFINED(timeout)) {
    timeout = INFINITY;
  } else {
    timeout = ToNumber(timeout);
    if (NUMBER_IS_NAN(timeout)) {
      timeout = INFINITY;
    } else {
      timeout = MathMax(0, timeout);
    }
  }
  return %AtomicsFutexWait(ia, index, value, timeout);
}

function AtomicsFutexWakeJS(ia, index, count) {
  CheckSharedInteger32TypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  count = MathMax(0, $toInteger(count));
  return %AtomicsFutexWake(ia, index, count);
}

function AtomicsFutexWakeOrRequeueJS(ia, index1, count, value, index2) {
  CheckSharedInteger32TypedArray(ia);
  index1 = $toInteger(index1);
  count = MathMax(0, $toInteger(count));
  value = TO_INT32(value);
  index2 = $toInteger(index2);
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

%AddNamedProperty(Atomics, symbolToStringTag, "Atomics", READ_ONLY | DONT_ENUM);

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
