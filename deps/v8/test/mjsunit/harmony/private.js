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

// Flags: --expose-gc --allow-natives-syntax

var symbols = []


// Returns true if the string is a valid
// serialization of Symbols added to the 'symbols'
// array. Adjust if you extend 'symbols' with other
// values.
function isValidSymbolString(s) {
  return ["Symbol(66)"].indexOf(s) >= 0;
}


// Test different forms of constructor calls, all equivalent.
function TestNew() {
  for (var i = 0; i < 2; ++i) {
    for (var j = 0; j < 5; ++j) {
      symbols.push(%CreatePrivateSymbol("66"))
      symbols.push(Object(%CreatePrivateSymbol("66")).valueOf())
    }
    gc()  // Promote existing symbols and then allocate some more.
  }
}
TestNew()


function TestType() {
  for (var i in symbols) {
    assertEquals("symbol", typeof symbols[i])
    assertTrue(typeof symbols[i] === "symbol")
    assertTrue(%SymbolIsPrivate(symbols[i]))
    assertEquals(null, %_ClassOf(symbols[i]))
    assertEquals("Symbol", %_ClassOf(Object(symbols[i])))
  }
}
TestType()


function TestPrototype() {
  for (var i in symbols) {
    assertSame(Symbol.prototype, symbols[i].__proto__)
  }
}
TestPrototype()


function TestConstructor() {
  for (var i in symbols) {
    assertSame(Symbol, symbols[i].__proto__.constructor)
    assertSame(Symbol, Object(symbols[i]).__proto__.constructor)
  }
}
TestConstructor()


function TestToString() {
  for (var i in symbols) {
    assertThrows(function() {new String(symbols[i]) }, TypeError)
    assertEquals(symbols[i].toString(), String(symbols[i]))
    assertThrows(function() { symbols[i] + "" }, TypeError)
    assertTrue(isValidSymbolString(symbols[i].toString()))
    assertTrue(isValidSymbolString(Object(symbols[i]).toString()))
    assertTrue(isValidSymbolString(Symbol.prototype.toString.call(symbols[i])))
    assertEquals(
        "[object Symbol]", Object.prototype.toString.call(symbols[i]))
  }
}
TestToString()


function TestToBoolean() {
  for (var i in symbols) {
    assertTrue(Boolean(symbols[i]).valueOf())
    assertFalse(!symbols[i])
    assertTrue(!!symbols[i])
    assertTrue(symbols[i] && true)
    assertFalse(!symbols[i] && false)
    assertTrue(!symbols[i] || true)
    assertEquals(1, symbols[i] ? 1 : 2)
    assertEquals(2, !symbols[i] ? 1 : 2)
    if (!symbols[i]) assertUnreachable();
    if (symbols[i]) {} else assertUnreachable();
  }
}
TestToBoolean()


function TestToNumber() {
  for (var i in symbols) {
    assertThrows(function() { Number(symbols[i]); }, TypeError);
    assertThrows(function() { symbols[i] + 0; }, TypeError);
  }
}
TestToNumber()


function TestEquality() {
  // Every symbol should equal itself, and non-strictly equal its wrapper.
  for (var i in symbols) {
    assertSame(symbols[i], symbols[i])
    assertEquals(symbols[i], symbols[i])
    assertTrue(Object.is(symbols[i], symbols[i]))
    assertTrue(symbols[i] === symbols[i])
    assertTrue(symbols[i] == symbols[i])
    assertFalse(symbols[i] === Object(symbols[i]))
    assertFalse(Object(symbols[i]) === symbols[i])
    assertFalse(symbols[i] == Object(symbols[i]))
    assertFalse(Object(symbols[i]) == symbols[i])
    assertTrue(symbols[i] === symbols[i].valueOf())
    assertTrue(symbols[i].valueOf() === symbols[i])
    assertTrue(symbols[i] == symbols[i].valueOf())
    assertTrue(symbols[i].valueOf() == symbols[i])
  }

  // All symbols should be distinct.
  for (var i = 0; i < symbols.length; ++i) {
    for (var j = i + 1; j < symbols.length; ++j) {
      assertFalse(Object.is(symbols[i], symbols[j]))
      assertFalse(symbols[i] === symbols[j])
      assertFalse(symbols[i] == symbols[j])
    }
  }

  // Symbols should not be equal to any other value (and the test terminates).
  var values = [347, 1.275, NaN, "string", null, undefined, {}, function() {}]
  for (var i in symbols) {
    for (var j in values) {
      assertFalse(symbols[i] === values[j])
      assertFalse(values[j] === symbols[i])
      assertFalse(symbols[i] == values[j])
      assertFalse(values[j] == symbols[i])
    }
  }
}
TestEquality()


function TestGet() {
  for (var i in symbols) {
    assertTrue(isValidSymbolString(symbols[i].toString()))
    assertEquals(symbols[i], symbols[i].valueOf())
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
    assertTrue(isValidSymbolString(symbols[i].toString()))
    symbols[i].valueOf = 0
    assertEquals(symbols[i], symbols[i].valueOf())
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
  assertTrue(%HasFastProperties(obj))
  // Set the even symbols via assignment.
  for (var i = 0; i < symbols.length; i += 2) {
    obj[symbols[i]] = i
    // Object should remain in fast mode until too many properties were added.
    assertTrue(%HasFastProperties(obj) || i >= 30)
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
    assertEquals("string", typeof name)
  }
}


function TestKeyNames(obj) {
  assertEquals(0, Object.keys(obj).length)

  var names = Object.getOwnPropertyNames(obj)
  for (var i in names) {
    assertEquals("string", typeof names[i])
  }
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


function TestCachedKeyAfterScavenge() {
  gc();
  // Keyed property lookup are cached.  Hereby we assume that the keys are
  // tenured, so that we only have to clear the cache between mark compacts,
  // but not between scavenges.  This must also apply for symbol keys.
  var key = Symbol("key");
  var a = {};
  a[key] = "abc";

  for (var i = 0; i < 100000; i++) {
    a[key] += "a";  // Allocations cause a scavenge.
  }
}
TestCachedKeyAfterScavenge();


function TestGetOwnPropertySymbols() {
  var privateSymbol = %CreatePrivateSymbol("private")
  var publicSymbol = Symbol()
  var publicSymbol2 = Symbol()
  var obj = {}
  obj[publicSymbol] = 1
  obj[privateSymbol] = 2
  obj[publicSymbol2] = 3
  var syms = Object.getOwnPropertySymbols(obj)
  assertEquals(syms, [publicSymbol, publicSymbol2])
}
TestGetOwnPropertySymbols()


function TestSealAndFreeze(freeze) {
  var sym = %CreatePrivateSymbol("private")
  var obj = {}
  obj[sym] = 1
  freeze(obj)
  obj[sym] = 2
  assertEquals(2, obj[sym])
  assertTrue(delete obj[sym])
  assertEquals(undefined, obj[sym])
}
TestSealAndFreeze(Object.seal)
TestSealAndFreeze(Object.freeze)
TestSealAndFreeze(Object.preventExtensions)
