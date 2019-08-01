// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

var global = this;
var unique_id = 0;
// Creates a function with unique SharedFunctionInfo to ensure the feedback
// vector is unique for each test case.
function MakeFunctionWithUniqueSFI(...args) {
  assertTrue(args.length > 0);
  var body = `/* Unique comment: ${unique_id++} */ ` + args.pop();
  return new Function(...args, body);
}


//
// Load constant field from constant object directly.
//
function TestLoadFromConstantFieldOfAConstantObject(the_value, other_value) {
  function A(v) { this.v = v; }
  function O() { this.a = new A(the_value); }
  var the_object = new O();

  // Ensure that {the_object.a}'s map is not stable to complicate compiler's
  // life.
  new A(the_value).blah = 0;

  // Ensure that constant tracking is enabled for {contant_object}.
  delete global.constant_object;
  global.constant_object = the_object;
  assertEquals(the_object, constant_object);

  assertTrue(%HasFastProperties(the_object));

  // {constant_object} is known to the compiler via global property cell
  // tracking.
  var load = MakeFunctionWithUniqueSFI("return constant_object.a.v;");
  %PrepareFunctionForOptimization(load);
  load();
  load();
  %OptimizeFunctionOnNextCall(load);
  assertEquals(the_value, load());
  assertOptimized(load);
  var a = new A(other_value);
  assertTrue(%HaveSameMap(a, the_object.a));
  // Make constant field mutable by assigning another value
  // to some other instance of A.
  new A(the_value).v = other_value;
  assertTrue(%HaveSameMap(a, new A(the_value)));
  assertTrue(%HaveSameMap(a, the_object.a));
  assertUnoptimized(load);
  assertEquals(the_value, load());
  assertUnoptimized(load);
  assertEquals(the_value, load());
}

// Test constant tracking with Smi value.
(function() {
  var the_value = 42;
  var other_value = 153;
  TestLoadFromConstantFieldOfAConstantObject(the_value, other_value);
})();

// Test constant tracking with double value.
(function() {
  var the_value = 0.9;
  var other_value = 0.42;
  TestLoadFromConstantFieldOfAConstantObject(the_value, other_value);
})();

// Test constant tracking with function value.
(function() {
  var the_value = function V() {};
  var other_value = function W() {};
  TestLoadFromConstantFieldOfAConstantObject(the_value, other_value);
})();

// Test constant tracking with heap object value.
(function() {
  function V() {}
  var the_value = new V();
  var other_value = new V();
  TestLoadFromConstantFieldOfAConstantObject(the_value, other_value);
})();


//
// Load constant field from a prototype.
//
function TestLoadFromConstantFieldOfAPrototype(the_value, other_value) {
  function Proto() { this.v = the_value; }
  var the_prototype = new Proto();

  function O() {}
  O.prototype = the_prototype;
  var the_object = new O();

  // Ensure O.prototype is in fast mode by loading from its field.
  function warmup() { return new O().v; }
  %EnsureFeedbackVectorForFunction(warmup);
  warmup(); warmup(); warmup();
  assertTrue(%HasFastProperties(O.prototype));

  // The parameter object is not constant but all the values have the same
  // map and therefore the compiler knows the prototype object and can
  // optimize load of "v".
  var load = MakeFunctionWithUniqueSFI("o", "return o.v;");
  %PrepareFunctionForOptimization(load);
  load(new O());
  load(new O());
  %OptimizeFunctionOnNextCall(load);
  assertEquals(the_value, load(new O()));
  assertOptimized(load);
  // Invalidation of mutability should trigger deoptimization with a
  // "field-owner" reason.
  the_prototype.v = other_value;
  assertUnoptimized(load);
}

// Test constant tracking with Smi value.
(function() {
  var the_value = 42;
  var other_value = 153;
  TestLoadFromConstantFieldOfAPrototype(the_value, other_value);
})();

// Test constant tracking with double value.
(function() {
  var the_value = 0.9;
  var other_value = 0.42;
  TestLoadFromConstantFieldOfAPrototype(the_value, other_value);
})();

// Test constant tracking with function value.
(function() {
  var the_value = function V() {};
  var other_value = function W() {};
  TestLoadFromConstantFieldOfAPrototype(the_value, other_value);
})();

// Test constant tracking with heap object value.
(function() {
  function V() {}
  var the_value = new V();
  var other_value = new V();
  TestLoadFromConstantFieldOfAPrototype(the_value, other_value);
})();


//
// Store to constant field of a constant object.
//
function TestStoreToConstantFieldOfConstantObject(the_value, other_value) {
  function A(v) { this.v = v; }
  function O() { this.a = new A(the_value); }
  var the_object = new O();

  // Ensure that {the_object.a}'s map is not stable to complicate compiler's
  // life.
  new A(the_value).blah = 0;

  // Ensure that constant tracking is enabled for {contant_object}.
  delete global.constant_object;
  global.constant_object = the_object;
  assertEquals(the_object, constant_object);

  assertTrue(%HasFastProperties(the_object));

  // {constant_object} is known to the compiler via global property cell
  // tracking.
  var store = MakeFunctionWithUniqueSFI("v", "constant_object.a.v = v;");
  %PrepareFunctionForOptimization(store);
  store(the_value);
  store(the_value);
  %OptimizeFunctionOnNextCall(store);
  store(the_value);
  assertEquals(the_value, constant_object.a.v);
  assertOptimized(store);
  // Storing of the same value does not deoptimize.
  store(the_value);
  assertEquals(the_value, constant_object.a.v);
  assertOptimized(store);

  var a = new A(other_value);

  if (typeof the_value == "function" || typeof the_value == "object") {
    // For heap object fields "field-owner" dependency is installed for
    // any access of the field, therefore making constant field mutable by
    // assigning other value to some other instance of A should already
    // trigger deoptimization.
    assertTrue(%HaveSameMap(a, the_object.a));
    new A(the_value).v = other_value;
    assertTrue(%HaveSameMap(a, new A(the_value)));
    assertTrue(%HaveSameMap(a, the_object.a));
    assertUnoptimized(store);
  } else {
    assertOptimized(store);
  }
  // Storing other value deoptimizes because of failed value check.
  store(other_value);
  assertUnoptimized(store);
  assertEquals(other_value, constant_object.a.v);
}

// Test constant tracking with Smi values.
(function() {
  var the_value = 42;
  var other_value = 153;
  TestStoreToConstantFieldOfConstantObject(the_value, other_value);
})();

// Test constant tracking with double values.
(function() {
  var the_value = 0.9;
  var other_value = 0.42
  TestStoreToConstantFieldOfConstantObject(the_value, other_value);
})();

// Test constant tracking with function values.
(function() {
  var the_value = function V() {};
  var other_value = function W() {};
  TestStoreToConstantFieldOfConstantObject(the_value, other_value);
})();

// Test constant tracking with heap object values.
(function() {
  function V() {}
  var the_value = new V();
  var other_value = new V();
  TestStoreToConstantFieldOfConstantObject(the_value, other_value);
})();
