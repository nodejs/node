// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

// Evil base class; the ctor always returns the object passed in.
class Base {
  constructor(object) {
    return object;
  }
}

// Case 1: Object literal + named property -> we create const properties.
function makeNamed(x) {
  return {a: x}; // DefineNamedOwnPropertyInLiteral
}

let o1 = makeNamed(1);
assertEquals(1, o1.a);
assertTrue(%HasOwnConstDataProperty(o1, "a"));

let o2 = makeNamed(2);
assertEquals(2, o2.a);
assertTrue(%HasOwnConstDataProperty(o2, "a"));

// Case 2: Class + named property -> overwriting an existing const property
// must invalidate constness.
class DefineNamed extends Base {
  a = 3; // DefineNamedOwnProperty
}

new DefineNamed(o2);
assertEquals(3, o2.a);
assertFalse(%HasOwnConstDataProperty(o2, "a"));
assertFalse(%HasOwnConstDataProperty(o1, "a"));

// Case 3: Object literal + keyed property -> we don't create const properties.
function makeKeyed(x) {
  return {0: x}; // DefineKeyedOwnProperty
}

let o3 = makeKeyed(1);
assertEquals(1, o3[0]);
assertFalse(%HasOwnConstDataProperty(o3, "0"));

let o4 = makeKeyed(2);
assertEquals(2, o4[0]);
assertFalse(%HasOwnConstDataProperty(o4, "0"));

// Case 4: Class + keyed property -> we don't create const properties, and
// overwriting an existing property keeps it non-const.
class DefineKeyed extends Base {
  0 = 3; // DefineKeyedOwnProperty
}

new DefineKeyed(o4);
assertEquals(3, o4[0]);
assertFalse(%HasOwnConstDataProperty(o4, "0"));

// Case 5: Object literal + computed named property -> we create cons
// properties, and overwriting via a class computed property must invalidate
// constness.
let key = "a";
function makeKeyedNamed(x) {
  return {[key]: x}; // DefineKeyedOwnPropertyInLiteral
}

let o5 = makeKeyedNamed(1);
assertEquals(1, o5.a);
assertTrue(%HasOwnConstDataProperty(o5, "a"));

let o6 = makeKeyedNamed(2);
assertEquals(2, o6.a);
assertTrue(%HasOwnConstDataProperty(o6, "a"));

// Case 6: Class + computed named property -> overwriting an existing const
// property must invalidate constness.
class DefineKeyedNamed extends Base {
  [key] = 3; // DefineKeyedOwnProperty
}

new DefineKeyedNamed(o6);
assertEquals(3, o6.a);
assertFalse(%HasOwnConstDataProperty(o6, "a"));
assertFalse(%HasOwnConstDataProperty(o5, "a"));
