// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax --ignition
// The functions used for testing backtraces. They are at the top to make the
// testing of source line/column easier.

// Get the Debug object exposed from the debug context global object.
var Debug = debug.Debug;

var test_name;
var exception;
var begin_test_count = 0;
var end_test_count = 0;

// Initialize for a new test.
function BeginTest(name) {
  test_name = name;
  exception = null;
  begin_test_count++;
}

// Check result of a test.
function EndTest() {
  assertNull(exception, test_name + " / " + exception);
  end_test_count++;
}

// Check that two scope are the same.
function assertScopeMirrorEquals(scope1, scope2) {
  assertEquals(scope1.scopeType(), scope2.scopeType());
  assertEquals(scope1.frameIndex(), scope2.frameIndex());
  assertEquals(scope1.scopeIndex(), scope2.scopeIndex());
  assertPropertiesEqual(scope1.scopeObject().value(),
                        scope2.scopeObject().value());
}

// Check that the scope chain contains the expected types of scopes.
function CheckScopeChain(scopes, gen_mirror) {
  var all_scopes = gen_mirror.allScopes();
  assertEquals(scopes.length, gen_mirror.scopeCount());
  assertEquals(scopes.length, all_scopes.length,
               "FrameMirror.allScopes length");
  for (var i = 0; i < scopes.length; i++) {
    var scope = gen_mirror.scope(i);
    assertTrue(scope.isScope());
    assertEquals(scopes[i], scope.scopeType(),
                 `Scope ${i} has unexpected type`);
    assertScopeMirrorEquals(all_scopes[i], scope);

    // Check the global object when hitting the global scope.
    if (scopes[i] == debug.ScopeType.Global) {
      // Objects don't have same class (one is "global", other is "Object",
      // so just check the properties directly.
      assertPropertiesEqual(this, scope.scopeObject().value());
    }
  }
}

// Check that the content of the scope is as expected. For functions just check
// that there is a function.
function CheckScopeContent(content, number, gen_mirror) {
  var scope = gen_mirror.scope(number);
  var count = 0;
  for (var p in content) {
    var property_mirror = scope.scopeObject().property(p);
    assertFalse(property_mirror.isUndefined(),
                'property ' + p + ' not found in scope');
    if (typeof(content[p]) === 'function') {
      assertTrue(property_mirror.value().isFunction());
    } else {
      assertEquals(content[p], property_mirror.value().value(),
                   'property ' + p + ' has unexpected value');
    }
    count++;
  }

  // 'arguments' and might be exposed in the local and closure scope. Just
  // ignore this.
  var scope_size = scope.scopeObject().properties().length;
  if (!scope.scopeObject().property('arguments').isUndefined()) {
    scope_size--;
  }
  // Ditto for 'this'.
  if (!scope.scopeObject().property('this').isUndefined()) {
    scope_size--;
  }
  // Temporary variables introduced by the parser have not been materialized.
  assertTrue(scope.scopeObject().property('').isUndefined());

  if (count != scope_size) {
    print('Names found in scope:');
    var names = scope.scopeObject().propertyNames();
    for (var i = 0; i < names.length; i++) {
      print(names[i]);
    }
  }
  assertEquals(count, scope_size);
}

// Simple empty closure scope.

function *gen1() {
  yield 1;
  return 2;
}

var g = gen1();
var gm = debug.MakeMirror(g);
CheckScopeChain([debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({}, 0, gm);

// Closure scope with a parameter.

function *gen2(a) {
  yield a;
  return 2;
}

g = gen2(42);
gm = debug.MakeMirror(g);
CheckScopeChain([debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({a: 42}, 0, gm);

// Closure scope with a parameter.

function *gen3(a) {
  var b = 1
  yield a;
  return b;
}

g = gen3(0);
gm = debug.MakeMirror(g);
CheckScopeChain([debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({a: 0, b: undefined}, 0, gm);

g.next();  // Create b.
CheckScopeContent({a: 0, b: 1}, 0, gm);

// Closure scope with a parameter.

function *gen4(a, b) {
  var x = 2;
  yield a;
  var y = 3;
  return b;
}

g = gen4(0, 1);
gm = debug.MakeMirror(g);
CheckScopeChain([debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({a: 0, b: 1, x: undefined, y: undefined}, 0, gm);

g.next();  // Create x.
CheckScopeContent({a: 0, b: 1, x: 2, y: undefined}, 0, gm);

g.next();  // Create y.
CheckScopeContent({a: 0, b: 1, x: 2, y: 3}, 0, gm);

// Closure introducing local variable using eval.

function *gen5(a) {
  eval('var b = 2');
  return b;
}

g = gen5(1);
g.next();
gm = debug.MakeMirror(g);
CheckScopeChain([debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({a: 1, b: 2}, 0, gm);

// Single empty with block.

function *gen6() {
  with({}) {
    yield 1;
  }
  yield 2;
  return 3;
}

g = gen6();
g.next();
gm = debug.MakeMirror(g);
CheckScopeChain([debug.ScopeType.With,
                 debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({}, 0, gm);

g.next();
CheckScopeChain([debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);

// Nested empty with blocks.

function *gen7() {
  with({}) {
    with({}) {
      yield 1;
    }
    yield 2;
  }
  return 3;
}

g = gen7();
g.next();
gm = debug.MakeMirror(g);
CheckScopeChain([debug.ScopeType.With,
                 debug.ScopeType.With,
                 debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({}, 0, gm);

// Nested with blocks using in-place object literals.

function *gen8() {
  with({a: 1,b: 2}) {
    with({a: 2,b: 1}) {
      yield a;
    }
    yield a;
  }
  return 3;
}

g = gen8();
g.next();
gm = debug.MakeMirror(g);
CheckScopeChain([debug.ScopeType.With,
                 debug.ScopeType.With,
                 debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({a: 2, b: 1}, 0, gm);

g.next();
CheckScopeContent({a: 1, b: 2}, 0, gm);

// Catch block.

function *gen9() {
  try {
    throw 42;
  } catch (e) {
    yield e;
  }
  return 3;
}

g = gen9();
g.next();
gm = debug.MakeMirror(g);
CheckScopeChain([debug.ScopeType.Catch,
                 debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({e: 42}, 0, gm);

// For statement with block scope.

function *gen10() {
  for (let i = 0; i < 42; i++) yield i;
  return 3;
}

g = gen10();
g.next();
gm = debug.MakeMirror(g);
CheckScopeChain([debug.ScopeType.Block,
                 debug.ScopeType.Block,
                 debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({i: 0}, 0, gm);

g.next();
CheckScopeContent({i: 1}, 0, gm);
CheckScopeContent({i: 0}, 1, gm);  // Additional block scope with i = 0;

// Nested generators.

var gen12;
function *gen11() {
  gen12 = function*() {
    var a = 1;
    yield 1;
    return 2;
  }();

  var a = 0;
  yield* gen12;
}

g = gen11();
g.next();

gm = debug.MakeMirror(gen12);
CheckScopeChain([debug.ScopeType.Closure,
                 debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({a: 1}, 0, gm);
CheckScopeContent({a: 0}, 1, gm);

// Set a variable in an empty scope.

function *gen13() {
  yield 1;
  return 2;
}

var g = gen13();
var gm = debug.MakeMirror(g);
assertThrows(() => gm.scope(0).setVariableValue("a", 42));
CheckScopeContent({}, 0, gm);

// Set a variable in a simple scope.

function *gen14() {
  var a = 0;
  yield 1;
  yield a;
  return 2;
}

var g = gen14();
assertEquals(1, g.next().value);

var gm = debug.MakeMirror(g);
CheckScopeContent({a: 0}, 0, gm);

gm.scope(0).setVariableValue("a", 1);
CheckScopeContent({a: 1}, 0, gm);

assertEquals(1, g.next().value);

// Set a variable in nested with blocks using in-place object literals.

function *gen15() {
  var c = 3;
  with({a: 1,b: 2}) {
    var d = 4;
    yield a;
    var e = 5;
  }
  yield e;
  return e;
}

var g = gen15();
assertEquals(1, g.next().value);

var gm = debug.MakeMirror(g);
CheckScopeChain([debug.ScopeType.With,
                 debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({a: 1, b: 2}, 0, gm);
CheckScopeContent({c: 3, d: 4, e: undefined}, 1, gm);

// Variables don't exist in given scope.
assertThrows(() => gm.scope(0).setVariableValue("c", 42));
assertThrows(() => gm.scope(1).setVariableValue("a", 42));

// Variables in with scope are immutable.
assertThrows(() => gm.scope(0).setVariableValue("a", 3));
assertThrows(() => gm.scope(0).setVariableValue("b", 3));

gm.scope(1).setVariableValue("c", 1);
gm.scope(1).setVariableValue("e", 42);

CheckScopeContent({a: 1, b: 2}, 0, gm);
CheckScopeContent({c: 1, d: 4, e: 42}, 1, gm);
assertEquals(5, g.next().value);  // Initialized after set.

CheckScopeChain([debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);

gm.scope(0).setVariableValue("e", 42);

CheckScopeContent({c: 1, d: 4, e: 42}, 0, gm);
assertEquals(42, g.next().value);

// Set a variable in nested with blocks using in-place object literals plus a
// nested block scope.

function *gen16() {
  var c = 3;
  with({a: 1,b: 2}) {
    let d = 4;
    yield a;
    let e = 5;
    yield d;
  }
  return 3;
}

var g = gen16();
g.next();

var gm = debug.MakeMirror(g);
CheckScopeChain([debug.ScopeType.Block,
                 debug.ScopeType.With,
                 debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({d: 4}, 0, gm);
CheckScopeContent({a: 1, b: 2}, 1, gm);
CheckScopeContent({c: 3}, 2, gm);

gm.scope(0).setVariableValue("d", 1);
CheckScopeContent({d: 1}, 0, gm);

assertEquals(1, g.next().value);

// Set variable in catch block.

var yyzyzzyz = 4829;
let xxxyyxxyx = 42284;
function *gen17() {
  try {
    throw 42;
  } catch (e) {
    yield e;
    yield e;
  }
  return 3;
}

g = gen17();
g.next();

gm = debug.MakeMirror(g);
CheckScopeChain([debug.ScopeType.Catch,
                 debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global], gm);
CheckScopeContent({e: 42}, 0, gm);
CheckScopeContent({xxxyyxxyx: 42284}, 2, gm);

gm.scope(0).setVariableValue("e", 1);
CheckScopeContent({e: 1}, 0, gm);

assertEquals(1, g.next().value);

// Script scope.
gm.scope(2).setVariableValue("xxxyyxxyx", 42);
assertEquals(42, xxxyyxxyx);

// Global scope.
assertThrows(() => gm.scope(3).setVariableValue("yyzyzzyz", 42));
assertEquals(4829, yyzyzzyz);
