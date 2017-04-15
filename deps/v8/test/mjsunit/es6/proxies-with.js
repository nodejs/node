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


// Helper.

function TestWithProxies(test, x, y, z) {
  test(function(h) { return new Proxy({}, h) }, x, y, z)
  test(function(h) {
      return new Proxy(function() {}, h)
  }, x, y, z)
}



// Getting.

function TestWithGet(handler) {
  TestWithProxies(TestWithGet2, handler)
}

var c = "global"
var key = ""

function TestWithGet2(create, handler) {
  var b = "local"

  var p = create(handler);
  assertEquals("onproxy", p.a);
  assertEquals(undefined, p.b);
  assertEquals(undefined, p.c);

  with (p) {
    assertEquals("onproxy", a);
    assertEquals("local", b);
    assertEquals("global", c);
  }

  var o = Object.create(p, {d: {value: "own"}})
  with (o) {
    assertEquals("onproxy", a)
    assertEquals("local", b);
    assertEquals("global", c)
    assertEquals("own", d)
  }
}

TestWithGet({
  get(target, k) {
    key = k;
    return k === "a" ? "onproxy" : undefined
  },
  has(target, k) { return k === 'a' }
})

TestWithGet({
  get: function(r, k) { return this.get2(r, k) },
  get2: function(r, k) { key = k; return k === "a" ? "onproxy" : undefined },
  has(target, k) { return k === 'a' }
})




// Invoking.

function TestWithGetCall(handler) {
  TestWithProxies(TestWithGetCall2, handler)
}

var receiver = null
var c = function() { return "global" }

function TestWithGetCall2(create, handler) {
  var b = function() { return "local" }

  var p = create(handler)
  with (p) {
    receiver = null
    assertEquals("onproxy", a())
    assertSame(p, receiver)
    assertEquals("local", b())
    assertEquals("global", c())
  }

  var o = Object.create(p, {d: {value: function() { return "own" }}})
  with (o) {
    receiver = null
    assertEquals("onproxy", a())
    assertSame(o, receiver)
    assertEquals("local", b())
    assertEquals("global", c())
    assertEquals("own", d())
  }
}

function onproxy() { receiver = this; return "onproxy" }

TestWithGetCall({
  get: function(r, k) { key = k; return k === "a" ? onproxy : undefined },
  has: function(t, k) {
    key = k;
    return k === "a";
  }
})

TestWithGetCall({
  get: function(r, k) { return this.get2(r, k) },
  get2: function(r, k) { key = k; return k === "a" ? onproxy : undefined },
  has: function(t, k) {
    key = k;
    return k === "a";
  }
})

TestWithGetCall({
  get: function(r, k) { key = k; return k === "a" ? onproxy : undefined },
  has: function(t, k) {
    return this.has2(k)
  },
  has2: function(k) {
    key = k;
    return k === "a";
  }
})

TestWithGetCall({
  get: function(r, k) { key = k; return k === "a" ? onproxy : undefined },
  has: function(t, k) {
    key = k;
    return k === "a";
  }
})


function TestWithGetCallThrow(handler) {
  TestWithProxies(TestWithGetCallThrow2, handler)
}

function TestWithGetCallThrow2(create, handler) {
  var b = function() { return "local" }

  var p = create(handler)
  with (p) {
    assertThrowsEquals(function(){ a() }, "myexn")
    assertEquals("local", b())
    assertEquals("global", c())
  }

  var o = Object.create(p, {d: {value: function() { return "own" }}})
  with (o) {
    assertThrowsEquals(function(){ a() }, "myexn")
    assertEquals("local", b())
    assertEquals("global", c())
    assertEquals("own", d())
  }
}

function onproxythrow() { throw "myexn" }

TestWithGetCallThrow({
  has: function(r, k) { return k === "a"; },
  get: function(r, k) { key = k; return k === "a" ? onproxythrow : undefined },
})

TestWithGetCallThrow({
  has: function(r, k) { return k === "a"; },
  get: function(r, k) { return this.get2(r, k) },
  get2: function(r, k) { key = k; return k === "a" ? onproxythrow : undefined },
})



// Setting.

var key
var val

function TestWithSet(handler, hasSetter) {
  TestWithProxies(TestWithSet2, handler, hasSetter)
}

var c = "global"

function TestWithSet2(create, handler, hasSetter) {
  var b = "local"

  var p = create(handler)
  key = val = undefined
  with (p) {
    a = "set"
    assertEquals("a", key)
    assertEquals("set", val)
    assertEquals("local", b)
    assertEquals("global", c)
    b = "local"
    c = "global"
    assertEquals("a", key)
    assertEquals("set", val)
  }

  if (!hasSetter) return

  var o = Object.create(p, {d: {value: "own"}})
  key = val = undefined
  with (o) {
    a = "set"
    assertEquals("a", key)
    assertEquals("set", val)
    assertEquals("local", b)
    assertEquals("global", c)
    assertEquals("own", d)
    b = "local"
    c = "global"
    d = "own"
    assertEquals("a", key)
    assertEquals("set", val)
  }
}

TestWithSet({
  set: function(r, k, v) { key = k; val = v; return true },
  has: function(t, k) {
    return k === "a"
  }
})

TestWithSet({
  set: function(r, k, v) { return this.set2(r, k, v) },
  set2: function(r, k, v) { key = k; val = v; return true },
  has: function(t, k) {
    return k === "a"
  }
})

TestWithSet({
  has: function(t, k) {
    return k === "a"
  },
  defineProperty: function(t, k, desc) { key = k; val = desc.value }
})

TestWithSet({
  has: function(t, k) {
    return this.has2(k)
  },
  has2: function(k) {
    return k === "a"
  },
  defineProperty: function(t, k, desc) { this.defineProperty2(k, desc) },
  defineProperty2: function(k, desc) { key = k; val = desc.value }
})

TestWithSet({
  has: function(t, k) {
    return k === "a"
  },
  defineProperty: function(t, k, desc) { key = k; val = desc.value }
})

TestWithSet({
  has: function(t, k) {
    return this.has2(k) },
  has2: function(k) {
    return k === "a"
  },
  set: function(t, k, v) { key = k; val = v; return true }
}, true)

TestWithSet({
  has: function(t, k) {
    return k === "a"
  },
  defineProperty: function(t, k, desc) { key = k; val = desc.value }
})


function TestWithSetThrow(handler, hasSetter) {
  TestWithProxies(TestWithSetThrow2, handler, hasSetter)
}

function TestWithSetThrow2(create, handler, hasSetter) {
  var p = create(handler)
  assertThrowsEquals(function(){
    with (p) {
      a = 1
    }
  }, "myexn")

  if (!hasSetter) return

  var o = Object.create(p, {})
  assertThrowsEquals(function(){
    with (o) {
      a = 1
    }
  }, "myexn")
}

TestWithSetThrow({
  set: function() { throw "myexn" },
  has: function(t, k) {
    return k === "a"
  }
})

TestWithSetThrow({
  has: function() { throw "myexn" },
})

TestWithSetThrow({
  has: function() { throw "myexn" },
})

TestWithSetThrow({
  has: function(t, k) {
    return k === "a"
  },
  defineProperty: function() { throw "myexn" }
})

TestWithSetThrow({
  has: function(t, k) {
    return k === "a"
  },
  set: function() { throw "myexn" }
}, true)
