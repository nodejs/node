// Copyright 2011 the V8 project authors. All rights reserved.
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

// Flags: --harmony-proxies --harmony-collections


// Helper.

function TestWithProxies(test, construct, handler) {
  test(construct, handler, Proxy.create)
  test(construct, handler, function(h) {
    return Proxy.createFunction(h, function() {})
  })
}


// Sets.

function TestSet(construct, fix) {
  TestWithProxies(TestSet2, construct, fix)
}

function TestSet2(construct, fix, create) {
  var handler = {fix: function() { return {} }}
  var p1 = create(handler)
  var p2 = create(handler)
  var p3 = create(handler)
  fix(p3)

  var s = construct();
  s.add(p1);
  s.add(p2);
  assertTrue(s.has(p1));
  assertTrue(s.has(p2));
  assertFalse(s.has(p3));

  fix(p1)
  fix(p2)
  assertTrue(s.has(p1));
  assertTrue(s.has(p2));
  assertFalse(s.has(p3));

  s.delete(p2);
  assertTrue(s.has(p1));
  assertFalse(s.has(p2));
  assertFalse(s.has(p3));
}

TestSet(Set, Object.seal)
TestSet(Set, Object.freeze)
TestSet(Set, Object.preventExtensions)


// Maps and weak maps.

function TestMap(construct, fix) {
  TestWithProxies(TestMap2, construct, fix)
}

function TestMap2(construct, fix, create) {
  var handler = {fix: function() { return {} }}
  var p1 = create(handler)
  var p2 = create(handler)
  var p3 = create(handler)
  fix(p3)

  var m = construct();
  m.set(p1, 123);
  m.set(p2, 321);
  assertTrue(m.has(p1));
  assertTrue(m.has(p2));
  assertFalse(m.has(p3));
  assertSame(123, m.get(p1));
  assertSame(321, m.get(p2));

  fix(p1)
  fix(p2)
  assertTrue(m.has(p1));
  assertTrue(m.has(p2));
  assertFalse(m.has(p3));
  assertSame(123, m.get(p1));
  assertSame(321, m.get(p2));

  m.delete(p2);
  assertTrue(m.has(p1));
  assertFalse(m.has(p2));
  assertFalse(m.has(p3));
  assertSame(123, m.get(p1));
  assertSame(undefined, m.get(p2));
}

TestMap(Map, Object.seal)
TestMap(Map, Object.freeze)
TestMap(Map, Object.preventExtensions)

TestMap(WeakMap, Object.seal)
TestMap(WeakMap, Object.freeze)
TestMap(WeakMap, Object.preventExtensions)
