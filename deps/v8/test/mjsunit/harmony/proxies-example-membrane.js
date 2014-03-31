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

// Flags: --harmony


// A simple no-op handler. Adapted from:
// http://wiki.ecmascript.org/doku.php?id=harmony:proxies#examplea_no-op_forwarding_proxy

function createHandler(obj) {
  return {
    getOwnPropertyDescriptor: function(name) {
      var desc = Object.getOwnPropertyDescriptor(obj, name);
      if (desc !== undefined) desc.configurable = true;
      return desc;
    },
    getPropertyDescriptor: function(name) {
      var desc = Object.getOwnPropertyDescriptor(obj, name);
      //var desc = Object.getPropertyDescriptor(obj, name);  // not in ES5
      if (desc !== undefined) desc.configurable = true;
      return desc;
    },
    getOwnPropertyNames: function() {
      return Object.getOwnPropertyNames(obj);
    },
    getPropertyNames: function() {
      return Object.getOwnPropertyNames(obj);
      //return Object.getPropertyNames(obj);  // not in ES5
    },
    defineProperty: function(name, desc) {
      Object.defineProperty(obj, name, desc);
    },
    delete: function(name) {
      return delete obj[name];
    },
    fix: function() {
      if (Object.isFrozen(obj)) {
        var result = {};
        Object.getOwnPropertyNames(obj).forEach(function(name) {
          result[name] = Object.getOwnPropertyDescriptor(obj, name);
        });
        return result;
      }
      // As long as obj is not frozen, the proxy won't allow itself to be fixed
      return undefined; // will cause a TypeError to be thrown
    },
    has: function(name) { return name in obj; },
    hasOwn: function(name) { return ({}).hasOwnProperty.call(obj, name); },
    get: function(receiver, name) { return obj[name]; },
    set: function(receiver, name, val) {
      obj[name] = val;  // bad behavior when set fails in sloppy mode
      return true;
    },
    enumerate: function() {
      var result = [];
      for (var name in obj) { result.push(name); };
      return result;
    },
    keys: function() { return Object.keys(obj); }
  };
}



// Auxiliary definitions enabling tracking of object identity in output.

var objectMap = new WeakMap;
var objectCounter = 0;

function registerObject(x, s) {
  if (x === Object(x) && !objectMap.has(x))
    objectMap.set(x, ++objectCounter + (s == undefined ? "" : ":" + s));
}

registerObject(this, "global");
registerObject(Object.prototype, "Object.prototype");

function str(x) {
  if (x === Object(x)) return "[" + typeof x + " " + objectMap.get(x) + "]";
  if (typeof x == "string") return "\"" + x + "\"";
  return "" + x;
}



// A simple membrane. Adapted from:
// http://wiki.ecmascript.org/doku.php?id=harmony:proxies#a_simple_membrane

function createSimpleMembrane(target) {
  var enabled = true;

  function wrap(obj) {
    registerObject(obj);
    print("wrap enter", str(obj));
    try {
      var x = wrap2(obj);
      registerObject(x, "wrapped");
      print("wrap exit", str(obj), "as", str(x));
      return x;
    } catch(e) {
      print("wrap exception", str(e));
      throw e;
    }
  }

  function wrap2(obj) {
    if (obj !== Object(obj)) {
      return obj;
    }

    function wrapCall(fun, that, args) {
      registerObject(that);
      print("wrapCall enter", fun, str(that));
      try {
        var x = wrapCall2(fun, that, args);
        print("wrapCall exit", fun, str(that), "returning", str(x));
        return x;
      } catch(e) {
        print("wrapCall exception", fun, str(that), str(e));
        throw e;
      }
    }

    function wrapCall2(fun, that, args) {
      if (!enabled) { throw new Error("disabled"); }
      try {
        return wrap(fun.apply(that, Array.prototype.map.call(args, wrap)));
      } catch (e) {
        throw wrap(e);
      }
    }

    var baseHandler = createHandler(obj);
    var handler = Proxy.create(Object.freeze({
      get: function(receiver, name) {
        return function() {
          var arg = (name === "get" || name == "set") ? arguments[1] : "";
          print("handler enter", name, arg);
          var x = wrapCall(baseHandler[name], baseHandler, arguments);
          print("handler exit", name, arg, "returning", str(x));
          return x;
        }
      }
    }));
    registerObject(baseHandler, "basehandler");
    registerObject(handler, "handler");

    if (typeof obj === "function") {
      function callTrap() {
        print("call trap enter", str(obj), str(this));
        var x = wrapCall(obj, wrap(this), arguments);
        print("call trap exit", str(obj), str(this), "returning", str(x));
        return x;
      }
      function constructTrap() {
        if (!enabled) { throw new Error("disabled"); }
        try {
          function forward(args) { return obj.apply(this, args) }
          return wrap(new forward(Array.prototype.map.call(arguments, wrap)));
        } catch (e) {
          throw wrap(e);
        }
      }
      return Proxy.createFunction(handler, callTrap, constructTrap);
    } else {
      var prototype = wrap(Object.getPrototypeOf(obj));
      return Proxy.create(handler, prototype);
    }
  }

  var gate = Object.freeze({
    enable: function() { enabled = true; },
    disable: function() { enabled = false; }
  });

  return Object.freeze({
    wrapper: wrap(target),
    gate: gate
  });
}


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
print("o =", str(o))
print("w =", str(w));

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


// An identity-preserving membrane. Adapted from:
// http://wiki.ecmascript.org/doku.php?id=harmony:proxies#an_identity-preserving_membrane

function createMembrane(wetTarget) {
  var wet2dry = new WeakMap();
  var dry2wet = new WeakMap();

  function asDry(obj) {
    registerObject(obj)
    print("asDry enter", str(obj))
    try {
      var x = asDry2(obj);
      registerObject(x, "dry");
      print("asDry exit", str(obj), "as", str(x));
      return x;
    } catch(e) {
      print("asDry exception", str(e));
      throw e;
    }
  }
  function asDry2(wet) {
    if (wet !== Object(wet)) {
      // primitives provide only irrevocable knowledge, so don't
      // bother wrapping it.
      return wet;
    }
    var dryResult = wet2dry.get(wet);
    if (dryResult) { return dryResult; }

    var wetHandler = createHandler(wet);
    var dryRevokeHandler = Proxy.create(Object.freeze({
      get: function(receiver, name) {
        return function() {
          var arg = (name === "get" || name == "set") ? arguments[1] : "";
          print("dry handler enter", name, arg);
          var optWetHandler = dry2wet.get(dryRevokeHandler);
          try {
            var x = asDry(optWetHandler[name].apply(
              optWetHandler, Array.prototype.map.call(arguments, asWet)));
            print("dry handler exit", name, arg, "returning", str(x));
            return x;
          } catch (eWet) {
            var x = asDry(eWet);
            print("dry handler exception", name, arg, "throwing", str(x));
            throw x;
          }
        };
      }
    }));
    dry2wet.set(dryRevokeHandler, wetHandler);

    if (typeof wet === "function") {
      function callTrap() {
        print("dry call trap enter", str(this));
        var x = asDry(wet.apply(
          asWet(this), Array.prototype.map.call(arguments, asWet)));
        print("dry call trap exit", str(this), "returning", str(x));
        return x;
      }
      function constructTrap() {
        function forward(args) { return wet.apply(this, args) }
        return asDry(new forward(Array.prototype.map.call(arguments, asWet)));
      }
      dryResult =
        Proxy.createFunction(dryRevokeHandler, callTrap, constructTrap);
    } else {
      dryResult =
        Proxy.create(dryRevokeHandler, asDry(Object.getPrototypeOf(wet)));
    }
    wet2dry.set(wet, dryResult);
    dry2wet.set(dryResult, wet);
    return dryResult;
  }

  function asWet(obj) {
    registerObject(obj)
    print("asWet enter", str(obj))
    try {
      var x = asWet2(obj)
      registerObject(x, "wet")
      print("asWet exit", str(obj), "as", str(x))
      return x
    } catch(e) {
      print("asWet exception", str(e))
      throw e
    }
  }
  function asWet2(dry) {
    if (dry !== Object(dry)) {
      // primitives provide only irrevocable knowledge, so don't
      // bother wrapping it.
      return dry;
    }
    var wetResult = dry2wet.get(dry);
    if (wetResult) { return wetResult; }

    var dryHandler = createHandler(dry);
    var wetRevokeHandler = Proxy.create(Object.freeze({
      get: function(receiver, name) {
        return function() {
          var arg = (name === "get" || name == "set") ? arguments[1] : "";
          print("wet handler enter", name, arg);
          var optDryHandler = wet2dry.get(wetRevokeHandler);
          try {
            var x = asWet(optDryHandler[name].apply(
              optDryHandler, Array.prototype.map.call(arguments, asDry)));
            print("wet handler exit", name, arg, "returning", str(x));
            return x;
          } catch (eDry) {
            var x = asWet(eDry);
            print("wet handler exception", name, arg, "throwing", str(x));
            throw x;
          }
        };
      }
    }));
    wet2dry.set(wetRevokeHandler, dryHandler);

    if (typeof dry === "function") {
      function callTrap() {
        print("wet call trap enter", str(this));
        var x = asWet(dry.apply(
          asDry(this), Array.prototype.map.call(arguments, asDry)));
        print("wet call trap exit", str(this), "returning", str(x));
        return x;
      }
      function constructTrap() {
        function forward(args) { return dry.apply(this, args) }
        return asWet(new forward(Array.prototype.map.call(arguments, asDry)));
      }
      wetResult =
        Proxy.createFunction(wetRevokeHandler, callTrap, constructTrap);
    } else {
      wetResult =
        Proxy.create(wetRevokeHandler, asWet(Object.getPrototypeOf(dry)));
    }
    dry2wet.set(dry, wetResult);
    wet2dry.set(wetResult, dry);
    return wetResult;
  }

  var gate = Object.freeze({
    revoke: function() {
      dry2wet = wet2dry = Object.freeze({
        get: function(key) { throw new Error("revoked"); },
        set: function(key, val) { throw new Error("revoked"); }
      });
    }
  });

  return Object.freeze({ wrapper: asDry(wetTarget), gate: gate });
}


var receiver
var argument
var o = {
  a: 6,
  b: {bb: 8},
  f: function(x) { receiver = this; argument = x; return x },
  g: function(x) { receiver = this; argument = x; return x.a },
  h: function(x) { receiver = this; argument = x; this.q = x },
  s: function(x) { receiver = this; argument = x; this.x = {y: x}; return this }
}
o[2] = {c: 7}
var m = createMembrane(o)
var w = m.wrapper
print("o =", str(o))
print("w =", str(w))

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
