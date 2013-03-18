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

// Flags: --harmony-symbols --harmony-collections
// Flags: --expose-gc --allow-natives-syntax

var symbols = []

// Test different forms of constructor calls, all equivalent.
function TestNew() {
  function IndirectSymbol() { return new Symbol }
  function indirect() { return new IndirectSymbol() }
  for (var i = 0; i < 10; ++i) {
    symbols.push(new Symbol)
    symbols.push(new Symbol())
    symbols.push(Symbol())
    symbols.push(indirect())
  }
  %OptimizeFunctionOnNextCall(indirect)
  indirect()  // Call once before GC throws away type feedback.
  gc()        // Promote existing symbols and then allocate some more.
  for (var i = 0; i < 10; ++i) {
    symbols.push(new Symbol)
    symbols.push(new Symbol())
    symbols.push(Symbol())
    symbols.push(indirect())
  }
}
TestNew()


function TestType() {
  for (var i in symbols) {
    assertTrue(%_IsSymbol(symbols[i]))
    assertEquals("object", typeof symbols[i])
    assertTrue(typeof symbols[i] === "object")
    assertEquals("[object Symbol]", Object.prototype.toString.call(symbols[i]))
  }
}
TestType()


function TestEquality() {
  // Every symbol should equal itself.
  for (var i in symbols) {
    assertSame(symbols[i], symbols[i])
    assertEquals(symbols[i], symbols[i])
    assertTrue(Object.is(symbols[i], symbols[i]))
    assertTrue(symbols[i] === symbols[i])
    assertTrue(symbols[i] == symbols[i])
  }

  // All symbols should be distinct.
  for (var i = 0; i < symbols.length; ++i) {
    for (var j = i + 1; j < symbols.length; ++j) {
      assertFalse(Object.is(symbols[i], symbols[j]))
      assertFalse(symbols[i] === symbols[j])
      assertFalse(symbols[i] == symbols[j])
    }
  }
}
TestEquality()


function TestGet() {
  for (var i in symbols) {
    assertEquals("[object Symbol]", symbols[i].toString())
    assertEquals(undefined, symbols[i].valueOf)
    assertEquals(undefined, symbols[i].a)
    assertEquals(undefined, symbols[i]["a" + "b"])
    assertEquals(undefined, symbols[i]["" + "1"])
    assertEquals(undefined, symbols[i][62])
  }
}
TestGet()


function TestSet() {
  for (var i in symbols) {
    symbols[i].toString = 0
    assertEquals("[object Symbol]", symbols[i].toString())
    symbols[i].a = 0
    assertEquals(undefined, symbols[i].a)
    symbols[i]["a" + "b"] = 0
    assertEquals(undefined, symbols[i]["a" + "b"])
    symbols[i][62] = 0
    assertEquals(undefined, symbols[i][62])
  }
}
TestSet()


function TestCollections() {
  var set = new Set
  var map = new Map
  var weakmap = new WeakMap
  for (var i in symbols) {
    set.add(symbols[i])
    map.set(symbols[i], i)
    weakmap.set(symbols[i], i)
  }
  assertEquals(symbols.length, set.size)
  assertEquals(symbols.length, map.size)
  for (var i in symbols) {
    assertTrue(set.has(symbols[i]))
    assertTrue(map.has(symbols[i]))
    assertTrue(weakmap.has(symbols[i]))
    assertEquals(i, map.get(symbols[i]))
    assertEquals(i, weakmap.get(symbols[i]))
  }
  for (var i in symbols) {
    assertTrue(set.delete(symbols[i]))
    assertTrue(map.delete(symbols[i]))
    assertTrue(weakmap.delete(symbols[i]))
  }
  assertEquals(0, set.size)
  assertEquals(0, map.size)
}
TestCollections()



function TestKeySet(obj) {
  // Set the even symbols via assignment.
  for (var i = 0; i < symbols.length; i += 2) {
    obj[symbols[i]] = i
  }
}


function TestKeyDefine(obj) {
  // Set the odd symbols via defineProperty (as non-enumerable).
  for (var i = 1; i < symbols.length; i += 2) {
    Object.defineProperty(obj, symbols[i], {value: i, configurable: true})
  }
}


function TestKeyGet(obj) {
  var obj2 = Object.create(obj)
  for (var i in symbols) {
    assertEquals(i|0, obj[symbols[i]])
    assertEquals(i|0, obj2[symbols[i]])
  }
}


function TestKeyHas() {
  for (var i in symbols) {
    assertTrue(symbols[i] in obj)
    assertTrue(Object.hasOwnProperty.call(obj, symbols[i]))
  }
}


function TestKeyEnum(obj) {
  for (var name in obj) {
    assertFalse(%_IsSymbol(name))
  }
}


function TestKeyNames(obj) {
  assertEquals(0, Object.keys(obj).length)

  var names = Object.getOwnPropertyNames(obj)
  assertTrue(symbols.length <= names.length)
  // TODO(rossberg): once we have iterators, the following would be:
  //   var expected = new Set(symbols)
  var expected = new Set
  for (var i = 0; i < symbols.length; ++i) expected.add(symbols[i])
  for (var i = 0; i < names.length; ++i) {
    var name = names[i]
    var asString = String(name)
    if (asString !== name) {
      assertEquals("[object Symbol]", asString)
      assertTrue(expected.has(name))
      expected.delete(name)
    }
  }
  assertEquals(0, expected.size)
}


function TestKeyDescriptor(obj) {
  for (var i in symbols) {
    var desc = Object.getOwnPropertyDescriptor(obj, symbols[i]);
    assertEquals(i|0, desc.value)
    assertTrue(desc.configurable)
    assertEquals(i % 2 == 0, desc.writable)
    assertEquals(i % 2 == 0, desc.enumerable)
    assertEquals(i % 2 == 0,
        Object.prototype.propertyIsEnumerable.call(obj, symbols[i]))
  }
}


function TestKeyDelete(obj) {
  for (var i in symbols) {
    delete obj[symbols[i]]
  }
  for (var i in symbols) {
    assertEquals(undefined, Object.getOwnPropertyDescriptor(obj, symbols[i]))
  }
}


var objs = [{}, [], Object.create(null), Object(1), new Map, function(){}]

for (var i in objs) {
  var obj = objs[i]
  TestKeySet(obj)
  TestKeyDefine(obj)
  TestKeyGet(obj)
  TestKeyHas(obj)
  TestKeyEnum(obj)
  TestKeyNames(obj)
  TestKeyDescriptor(obj)
  TestKeyDelete(obj)
}
