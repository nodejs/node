// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

// Coverage for the has (`in`) megamorphic stub cache. Once an `in` site turns
// MEGAMORPHIC it dispatches through KeyedHasIC_Megamorphic, which probes
// isolate->has_stub_cache() instead of always calling the generic HasProperty.
// Most cases below are checked in every cache state: warm, cold after an
// explicit flush, and cold after a major GC.

// Above the polymorphic map limit, so every `in` site below reaches
// MEGAMORPHIC instead of POLYMORPHIC.
const kShapes = 12;

const objs = [];
for (let i = 0; i < kShapes; i++) {
  const C = new Function(`return class C${i} {
    constructor() { this.f${i} = ${i}; this.common = 1; }
  }`)();
  objs.push(new C());
}

// Feeds every shape to `fn` so its IC site goes megamorphic.
function drive(fn) {
  %PrepareFunctionForOptimization(fn);
  for (const o of objs) fn(o);
  return fn;
}

function assertMegamorphic(fn) {
  const feedback = %GetFeedback(fn);
  // Feedback-to-string is only available in OBJECT_PRINT builds.
  if (feedback === undefined) return;
  const slot = feedback.find(entry => entry[0] === 'HasKeyed');
  assertNotEquals(undefined, slot, `no HasKeyed slot in ${fn.name}`);
  assertEquals('MEGAMORPHIC', slot[1], `${fn.name} is not megamorphic`);
}

function megamorphic(fn) {
  drive(fn);
  assertMegamorphic(fn);
  return fn;
}

// The answer must not depend on whether the entry is currently cached.
function checkEveryCacheState(expected, fn, arg) {
  assertEquals(expected, fn(arg));  // warm: probe hits.
  %ClearMegamorphicStubCache();
  assertEquals(expected, fn(arg));  // cold: miss -> runtime -> refill.
  assertEquals(expected, fn(arg));  // warm again.
  gc();                             // major GC drops every stub cache entry.
  assertEquals(expected, fn(arg));
}

const hasTarget = megamorphic(function hasTarget(o) {
  return 'target' in o;
});
const hasCommon = megamorphic(function hasCommon(o) {
  return 'common' in o;
});
const hasProto = megamorphic(function hasProto(o) {
  return 'protoProp' in o;
});

// Own data property (kField) vs. absent property (kNonExistent).
for (const o of objs) {
  checkEveryCacheState(true, hasCommon, o);
  checkEveryCacheState(false, hasTarget, o);
}

// A cached kNonExistent handler carries the prototype chain validity cell;
// mutating the prototype must invalidate it even though the receiver's map is
// unchanged.
const proto = Object.getPrototypeOf(objs[1]);
assertFalse(hasProto(objs[1]));
proto.protoProp = 7;
checkEveryCacheState(true, hasProto, objs[1]);
delete proto.protoProp;
checkEveryCacheState(false, hasProto, objs[1]);

// Adding/removing an own property transitions the map, so the cached entry is
// keyed by a map the receiver no longer has.
objs[0].target = 42;
checkEveryCacheState(true, hasTarget, objs[0]);
delete objs[0].target;  // Transitions to dictionary mode.
checkEveryCacheState(false, hasTarget, objs[0]);

// A deprecated map must not be served a stale entry.
function Point(x) {
  this.x = x;
}
const smiPoint = new Point(1);
new Point(1.5);  // Generalizes `x` to Double, deprecating smiPoint's map.
const hasX = megamorphic(function hasX(o) {
  return 'x' in o;
});
checkEveryCacheState(true, hasX, smiPoint);

// Has handlers must never be served out of the *load* stub cache. For
// `f.prototype` and `wrapper.length` the load IC installs code handlers
// (LoadIC_FunctionPrototype, LoadIC_StringWrapperLength) that return the value
// rather than a boolean, and LoadIC::ComputeHandler deliberately skips them for
// has ICs. If both kinds shared one cache, the interleaved `in` below would
// return the loaded value.
function Fn() {}
const wrapper = new String('abc');
const hasPrototype = megamorphic(function hasPrototype(o) {
  return 'prototype' in o;
});
const hasLength = megamorphic(function hasLength(o) {
  return 'length' in o;
});
const loadPrototype = drive(function loadPrototype(o) {
  return o.prototype;
});
const loadLength = drive(function loadLength(o) {
  return o.length;
});
for (let i = 0; i < 10; i++) {
  assertSame(Fn.prototype, loadPrototype(Fn));
  assertTrue(hasPrototype(Fn));
  assertEquals(3, loadLength(wrapper));
  assertTrue(hasLength(wrapper));
}
checkEveryCacheState(true, hasPrototype, Fn);
checkEveryCacheState(true, hasLength, wrapper);

// `in` checks existence only: a cached accessor handler must not run the
// getter. Non-enumerable and inherited properties still count.
let getterCalls = 0;
class WithAccessor {
  get target() {
    getterCalls++;
    return 1;
  }
}
const withAccessor = new WithAccessor();
checkEveryCacheState(true, hasTarget, withAccessor);
assertEquals(0, getterCalls);

const nonEnum = {};
Object.defineProperty(nonEnum, 'target', {value: 1, enumerable: false});
checkEveryCacheState(true, hasTarget, nonEnum);
checkEveryCacheState(true, hasTarget, Object.create({target: 1}));
checkEveryCacheState(false, hasTarget, Object.create(null));

// Dictionary-mode objects share one map, so the cache key (map, name) cannot
// decide the answer: the kNormal handler has to consult the receiver's own
// property dictionary.
function makeDict(withTarget) {
  const o = {};
  for (let i = 0; i < 2000; i++) o['k' + i] = i;
  delete o.k0;
  if (withTarget) o.target = 1;
  return o;
}
const dictWith = makeDict(true);
const dictWithout = makeDict(false);
assertFalse(%HasFastProperties(dictWith));
assertFalse(%HasFastProperties(dictWithout));
assertTrue(%HaveSameMap(dictWith, dictWithout));
for (let i = 0; i < 10; i++) {
  assertTrue(hasTarget(dictWith));
  assertFalse(hasTarget(dictWithout));
}
checkEveryCacheState(true, hasTarget, dictWith);
checkEveryCacheState(false, hasTarget, dictWithout);
delete dictWith.target;
checkEveryCacheState(false, hasTarget, dictWith);

// Global object: the kGlobal handler has to check the property cell for the
// hole instead of trusting the map.
assertFalse(hasTarget(globalThis));
globalThis.target = 1;
checkEveryCacheState(true, hasTarget, globalThis);
delete globalThis.target;
checkEveryCacheState(false, hasTarget, globalThis);

// Proxies: the kProxy handler tail-calls the trap, so a cache hit must still
// run user code on every execution.
let trapCalls = 0;
const proxy = new Proxy({}, {
  has(t, k) {
    trapCalls++;
    return k === 'target';
  }
});
for (let i = 0; i < 100; i++) {
  assertTrue(hasTarget(proxy));
  assertFalse(hasCommon(proxy));
}
assertEquals(200, trapCalls);

// Symbols are unique names and go through the cache like strings.
const sym = Symbol('s');
const hasSym = megamorphic(function hasSym(o) {
  return sym in o;
});
checkEveryCacheState(true, hasSym, {[sym]: 1});
checkEveryCacheState(false, hasSym, {});

// Private names (`#field in o` and method brand checks) are private symbols
// and also go through the cache. Missing ones get a slow handler, and a proxy
// never sees a trap for them.
class Branded {
  #field = 1;
  #method() {}
  static hasField(o) {
    return #field in o;
  }
  static hasBrand(o) {
    return #method in o;
  }
}
const hasField = megamorphic(Branded.hasField);
const hasBrand = drive(Branded.hasBrand);
const branded = new Branded();
for (const o of [objs[0], objs[kShapes - 1]]) {
  checkEveryCacheState(false, hasField, o);
  checkEveryCacheState(false, hasBrand, o);
}
checkEveryCacheState(true, hasField, branded);
checkEveryCacheState(true, hasBrand, branded);
checkEveryCacheState(false, hasField, proxy);
assertEquals(200, trapCalls);

// Without a feedback vector (lazy feedback allocation) the has IC still lands
// in the megamorphic builtin; a miss there must fall back to HasProperty.
// The first invocations run without a vector, so flush before them.
function hasCommonNoVector(o) {
  return 'common' in o;
}
%ClearMegamorphicStubCache();
for (const o of objs) assertTrue(hasCommonNoVector(o));
assertFalse(hasCommonNoVector({}));

// Everything below must bypass the probe and fall through to HasProperty,
// because the stub cache is only keyed by (JSReceiver map, unique non-index
// name).

// Smi keys and index keys are element lookups, not named lookups.
let kKey = 'common';
const hasKey = megamorphic(function hasKey(o) {
  return kKey in o;
});
const arr = [1, 2, 3];
kKey = 0;
assertTrue(hasKey(arr));
kKey = 5;
assertFalse(hasKey(arr));
const ta = new Int32Array(4);
kKey = 0;
assertTrue(hasKey(ta));
kKey = 9;
assertFalse(hasKey(ta));
kKey = 'length';
assertTrue(hasKey(arr));

// An index *string* is not a unique name either: '5' must find element 5, not
// a named property.
const withNumericKey = {
  '5': 1
};
kKey = '5';
assertTrue(hasKey(withNumericKey));
assertTrue(hasKey(arr.concat([4, 5, 6])));
kKey = '6';
assertFalse(hasKey(withNumericKey));

// A freshly built, non-internalized string never reaches the cache.
for (let i = 0; i < 10; i++) {
  kKey = 'comm' + String.fromCharCode(111) + 'n';
  assertTrue(hasKey(objs[2]));
  kKey = 'targ' + String.fromCharCode(101) + 't';
  assertFalse(hasKey(objs[2]));
}

// Non-JSReceiver receivers must throw, not probe.
for (const prim of [1, 1.5, 'str', true, Symbol(), null, undefined]) {
  assertThrows(() => hasTarget(prim), TypeError);
}
