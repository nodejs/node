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


// A simple membrane. Adapted from:
// http://wiki.ecmascript.org/doku.php?id=harmony:proxies#a_simple_membrane

function createSimpleMembrane(target) {
  let enabled = true;

  function wrap(obj) {
    if (obj !== Object(obj)) return obj;

    let handler = new Proxy({}, {get: function(_, key) {
      if (!enabled) throw new Error("disabled");
      switch (key) {
      case "apply":
        return (_, that, args) => {
          try {
            return wrap(Reflect.apply(
                obj, wrap(that), args.map((x) => wrap(x))));
          } catch(e) {
            throw wrap(e);
          }
        }
      case "construct":
        return (_, args, newt) => {
          try {
            return wrap(Reflect.construct(
                obj, args.map((x) => wrap(x)), wrap(newt)));
          } catch(e) {
            throw wrap(e);
          }
        }
      default:
        return (_, ...args) => {
          try {
            return wrap(Reflect[key](obj, ...(args.map(wrap))));
          } catch(e) {
            throw wrap(e);
          }
        }
      }
    }});

    return new Proxy(obj, handler);
  }

  const gate = Object.freeze({
    enable: () => enabled = true,
    disable: () => enabled = false
  });

  return Object.freeze({
    wrapper: wrap(target),
    gate: gate
  });
}


// Test the simple membrane.
{
  var o = {
    a: 6,
    b: {bb: 8},
    f: function(x) { return x },
    g: function(x) { return x.a },
    h: function(x) { this.q = x }
  };
  o[2] = {c: 7};
  var m = createSimpleMembrane(o);
  var w = m.wrapper;
  var f = w.f;
  var x = f(66);
  var x = f({a: 1});
  var x = w.f({a: 1});
  var a = x.a;
  assertEquals(6, w.a);
  assertEquals(8, w.b.bb);
  assertEquals(7, w[2]["c"]);
  assertEquals(undefined, w.c);
  assertEquals(1, w.f(1));
  assertEquals(1, w.f({a: 1}).a);
  assertEquals(2, w.g({a: 2}));
  assertEquals(3, (w.r = {a: 3}).a);
  assertEquals(3, w.r.a);
  assertEquals(3, o.r.a);
  w.h(3);
  assertEquals(3, w.q);
  assertEquals(3, o.q);
  assertEquals(4, (new w.h(4)).q);

  var wb = w.b;
  var wr = w.r;
  var wf = w.f;
  var wf3 = w.f(3);
  var wfx = w.f({a: 6});
  var wgx = w.g({a: {aa: 7}});
  var wh4 = new w.h(4);
  m.gate.disable();
  assertEquals(3, wf3);
  assertThrows(function() { w.a }, Error);
  assertThrows(function() { w.r }, Error);
  assertThrows(function() { w.r = {a: 4} }, Error);
  assertThrows(function() { o.r.a }, Error);
  assertEquals("object", typeof o.r);
  assertEquals(5, (o.r = {a: 5}).a);
  assertEquals(5, o.r.a);
  assertThrows(function() { w[1] }, Error);
  assertThrows(function() { w.c }, Error);
  assertThrows(function() { wb.bb }, Error);
  assertThrows(function() { wr.a }, Error);
  assertThrows(function() { wf(4) }, Error);
  assertThrows(function() { wfx.a }, Error);
  assertThrows(function() { wgx.aa }, Error);
  assertThrows(function() { wh4.q }, Error);

  m.gate.enable();
  assertEquals(6, w.a);
  assertEquals(5, w.r.a);
  assertEquals(5, o.r.a);
  assertEquals(7, w.r = 7);
  assertEquals(7, w.r);
  assertEquals(7, o.r);
  assertEquals(8, w.b.bb);
  assertEquals(7, w[2]["c"]);
  assertEquals(undefined, w.c);
  assertEquals(8, wb.bb);
  assertEquals(3, wr.a);
  assertEquals(4, wf(4));
  assertEquals(3, wf3);
  assertEquals(6, wfx.a);
  assertEquals(7, wgx.aa);
  assertEquals(4, wh4.q);
}



// An identity-preserving membrane. Adapted from:
// http://wiki.ecmascript.org/doku.php?id=harmony:proxies#an_identity-preserving_membrane

function createMembrane(target) {
  const wet2dry = 0;
  const dry2wet = 1;

  function flip(dir) { return (dir + 1) % 2 }

  let maps = [new WeakMap(), new WeakMap()];

  let revoked = false;

  function wrap(dir, obj) {
    if (obj !== Object(obj)) return obj;

    let wrapper = maps[dir].get(obj);
    if (wrapper) return wrapper;

    let handler = new Proxy({}, {get: function(_, key) {
      if (revoked) throw new Error("revoked");
      switch (key) {
      case "apply":
        return (_, that, args) => {
          try {
            return wrap(dir, Reflect.apply(
                obj, wrap(flip(dir), that),
                args.map((x) => wrap(flip(dir), x))));
          } catch(e) {
            throw wrap(dir, e);
          }
        }
      case "construct":
        return (_, args, newt) => {
          try {
            return wrap(dir, Reflect.construct(
                obj, args.map((x) => wrap(flip(dir), x)),
                wrap(flip(dir), newt)));
          } catch(e) {
            throw wrap(dir, e);
          }
        }
      default:
        return (_, ...args) => {
          try {
            return wrap(dir, Reflect[key](
                obj, ...(args.map((x) => wrap(flip(dir), x)))))
          } catch(e) {
            throw wrap(dir, e);
          }
        }
      }
    }});

    wrapper = new Proxy(obj, handler);
    maps[dir].set(obj, wrapper);
    maps[flip(dir)].set(wrapper, obj);
    return wrapper;
  }

  const gate = Object.freeze({
    revoke: () => revoked = true
  });

  return Object.freeze({
    wrapper: wrap(wet2dry, target),
    gate: gate
  });
}


// Test the identity-preserving membrane.
{
  var receiver
  var argument
  var o = {
    a: 6,
    b: {bb: 8},
    f: function(x) {receiver = this; argument = x; return x},
    g: function(x) {receiver = this; argument = x; return x.a},
    h: function(x) {receiver = this; argument = x; this.q = x},
    s: function(x) {receiver = this; argument = x; this.x = {y: x}; return this}
  }
  o[2] = {c: 7}
  var m = createMembrane(o)
  var w = m.wrapper
  var f = w.f
  var x = f(66)
  var x = f({a: 1})
  var x = w.f({a: 1})
  var a = x.a
  assertEquals(6, w.a)
  assertEquals(8, w.b.bb)
  assertEquals(7, w[2]["c"])
  assertEquals(undefined, w.c)
  assertEquals(1, w.f(1))
  assertSame(o, receiver)
  assertEquals(1, w.f({a: 1}).a)
  assertSame(o, receiver)
  assertEquals(2, w.g({a: 2}))
  assertSame(o, receiver)
  assertSame(w, w.f(w))
  assertSame(o, receiver)
  assertSame(o, argument)
  assertSame(o, w.f(o))
  assertSame(o, receiver)
  // Note that argument !== o, since o isn't dry, so gets wrapped wet again.
  assertEquals(3, (w.r = {a: 3}).a)
  assertEquals(3, w.r.a)
  assertEquals(3, o.r.a)
  w.h(3)
  assertEquals(3, w.q)
  assertEquals(3, o.q)
  assertEquals(4, (new w.h(4)).q)
  assertEquals(5, w.s(5).x.y)
  assertSame(o, receiver)

  var wb = w.b
  var wr = w.r
  var wf = w.f
  var wf3 = w.f(3)
  var wfx = w.f({a: 6})
  var wgx = w.g({a: {aa: 7}})
  var wh4 = new w.h(4)
  var ws5 = w.s(5)
  var ws5x = ws5.x
  m.gate.revoke()
  assertEquals(3, wf3)
  assertThrows(function() { w.a }, Error)
  assertThrows(function() { w.r }, Error)
  assertThrows(function() { w.r = {a: 4} }, Error)
  assertThrows(function() { o.r.a }, Error)
  assertEquals("object", typeof o.r)
  assertEquals(5, (o.r = {a: 5}).a)
  assertEquals(5, o.r.a)
  assertThrows(function() { w[1] }, Error)
  assertThrows(function() { w.c }, Error)
  assertThrows(function() { wb.bb }, Error)
  assertEquals(3, wr.a)
  assertThrows(function() { wf(4) }, Error)
  assertEquals(6, wfx.a)
  assertEquals(7, wgx.aa)
  assertThrows(function() { wh4.q }, Error)
  assertThrows(function() { ws5.x }, Error)
  assertThrows(function() { ws5x.y }, Error)
}
