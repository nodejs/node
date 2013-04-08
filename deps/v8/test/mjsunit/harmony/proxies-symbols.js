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

// Flags: --harmony-proxies --harmony-symbols


// Helper.

function TestWithProxies(test, x, y, z) {
  test(Proxy.create, x, y, z)
  test(function(h) {return Proxy.createFunction(h, function() {})}, x, y, z)
}


// No symbols should leak to proxy traps.

function TestNoSymbolsToTrap(handler) {
  TestWithProxies(TestNoSymbolsToTrap2, handler)
}

function TestNoSymbolsToTrap2(create, handler) {
  var p = create(handler)
  var o = Object.create(p)
  var symbol = Symbol("secret")

  assertFalse(symbol in p)
  assertFalse(symbol in o)
  assertEquals(undefined, p[symbol])
  assertEquals(undefined, o[symbol])
  assertEquals(47, p[symbol] = 47)
  assertEquals(47, o[symbol] = 47)
  assertFalse(delete p[symbol])
  assertTrue(delete o[symbol])
  assertTrue(delete o[symbol])
  assertFalse({}.hasOwnProperty.call(p, symbol))
  assertFalse({}.hasOwnProperty.call(o, symbol))
  assertEquals(undefined, Object.getOwnPropertyDescriptor(p, symbol))
  assertEquals(undefined, Object.getOwnPropertyDescriptor(o, symbol))
}


TestNoSymbolsToTrap({
  has: assertUnreachable,
  hasOwn: assertUnreachable,
  get: assertUnreachable,
  set: assertUnreachable,
  delete: assertUnreachable,
  getPropertyDescriptor: assertUnreachable,
  getOwnPropertyDescriptor: assertUnreachable,
  defineProperty: assertUnreachable
})


// All symbols returned from proxy traps should be filtered.

function TestNoSymbolsFromTrap(handler) {
  TestWithProxies(TestNoSymbolsFromTrap2, handler)
}

function TestNoSymbolsFromTrap2(create, handler) {
  var p = create(handler)
  var o = Object.create(p)

  assertEquals(0, Object.keys(p).length)
  assertEquals(0, Object.keys(o).length)
  assertEquals(0, Object.getOwnPropertyNames(p).length)
  assertEquals(0, Object.getOwnPropertyNames(o).length)
  for (var n in p) assertUnreachable()
  for (var n in o) assertUnreachable()
}


function MakeSymbolArray() {
  return [Symbol(), Symbol("a")]
}

TestNoSymbolsFromTrap({
  enumerate: MakeSymbolArray,
  keys: MakeSymbolArray,
  getPropertyNames: MakeSymbolArray,
  getOwnPropertyNames: MakeSymbolArray
})
