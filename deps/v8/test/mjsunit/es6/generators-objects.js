// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax

// Test instantations of generators.

// Generators shouldn't allocate stack slots.  This test will abort in debug
// mode if generators have stack slots.
function TestContextAllocation() {
  function* g1(a, b, c) { yield 1; return [a, b, c]; }
  function* g2() { yield 1; return arguments; }
  function* g3() { yield 1; return this; }
  function* g4() { var x = 10; yield 1; return x; }
  // Temporary variable context allocation
  function* g5(l) { "use strict"; yield 1; for (let x in l) { yield x; } }

  g1();
  g2();
  g3();
  g4();
  g5(["foo"]);
}
TestContextAllocation();


// Test the properties and prototype of a generator object.
function TestGeneratorObject() {
  function* g() { yield 1; }

  var iter = g();
  assertSame(g.prototype, Object.getPrototypeOf(iter));
  assertTrue(iter instanceof g);
  assertEquals("Generator", %_ClassOf(iter));
  assertEquals("[object Generator]", String(iter));
  assertEquals([], Object.getOwnPropertyNames(iter));
  assertTrue(iter !== g());
  assertEquals("[object Generator]", Object.prototype.toString.call(iter));
  var gf = iter.__proto__.constructor;
  assertEquals("[object GeneratorFunction]", Object.prototype.toString.call(gf));

  // generators are not constructable.
  assertThrows(()=>new g());
}
TestGeneratorObject();


// Test the methods of generator objects.
function TestGeneratorObjectMethods() {
  function* g() { yield 1; }
  var iter = g();

  function TestNonGenerator(non_generator) {
    assertThrows(function() { iter.next.call(non_generator); }, TypeError);
    assertThrows(function() { iter.next.call(non_generator, 1); }, TypeError);
    assertThrows(function() { iter.throw.call(non_generator, 1); }, TypeError);
  }

  TestNonGenerator(1);
  TestNonGenerator({});
  TestNonGenerator(function(){});
  TestNonGenerator(g);
  TestNonGenerator(g.prototype);
}
TestGeneratorObjectMethods();


function TestPrototype() {
  function* g() { }

  let g_prototype = g.prototype;
  assertEquals([], Reflect.ownKeys(g_prototype));

  let generator_prototype = Object.getPrototypeOf(g_prototype);
  assertSame(generator_prototype, Object.getPrototypeOf(g).prototype);

  // Unchanged .prototype
  assertSame(g_prototype, Object.getPrototypeOf(g()));

  // Custom object as .prototype
  {
    let proto = {};
    g.prototype = proto;
    assertSame(proto, Object.getPrototypeOf(g()));
  }

  // Custom non-object as .prototype
  g.prototype = null;
  assertSame(generator_prototype, Object.getPrototypeOf(g()));
}
TestPrototype();


function TestComputedPropertyNames() {
  function* f1() { return {[yield]: 42} }
  var g1 = f1();
  g1.next();
  assertEquals(42, g1.next('a').value.a);

  function* f2() { return {['a']: yield} }
  var g2 = f2();
  g2.next();
  assertEquals(42, g2.next(42).value.a);
}
TestComputedPropertyNames();
