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

// Flags: --harmony-proxies --harmony-weakmaps


// Helper.

function TestWithProxies(test, handler) {
  test(handler, Proxy.create)
  test(handler, function(h) {return Proxy.createFunction(h, function() {})})
}


// Weak maps.

function TestWeakMap(fix) {
  TestWithProxies(TestWeakMap2, fix)
}

function TestWeakMap2(fix, create) {
  var handler = {fix: function() { return {} }}
  var p1 = create(handler)
  var p2 = create(handler)
  var p3 = create(handler)
  fix(p3)

  var m = new WeakMap
  m.set(p1, 123);
  m.set(p2, 321);
  assertSame(123, m.get(p1));
  assertSame(321, m.get(p2));

  fix(p1)
  fix(p2)
  assertSame(123, m.get(p1));
  assertSame(321, m.get(p2));
}

TestWeakMap(Object.seal)
TestWeakMap(Object.freeze)
TestWeakMap(Object.preventExtensions)
