// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-ayle license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --track-fields --expose-gc

var global = Function('return this')();
var verbose = 0;

function test(ctor_desc, use_desc, migr_desc) {
  var n = 5;
  var objects = [];
  var results = [];

  if (verbose) {
    print();
    print("===========================================================");
    print("=== " + ctor_desc.name +
          " | " + use_desc.name + " |--> " + migr_desc.name);
    print("===========================================================");
  }

  // Clean ICs and transitions.
  %NotifyContextDisposed();
  gc(); gc(); gc();


  // create objects
  if (verbose) {
    print("-----------------------------");
    print("--- construct");
    print();
  }
  for (var i = 0; i < n; i++) {
    objects[i] = ctor_desc.ctor.apply(ctor_desc, ctor_desc.args(i));
  }

  try {
    // use them
    if (verbose) {
      print("-----------------------------");
      print("--- use 1");
      print();
    }
    var use = use_desc.use1;
    for (var i = 0; i < n; i++) {
      if (i == 3) %OptimizeFunctionOnNextCall(use);
      results[i] = use(objects[i], i);
    }

    // trigger migrations
    if (verbose) {
      print("-----------------------------");
      print("--- trigger migration");
      print();
    }
    var migr = migr_desc.migr;
    for (var i = 0; i < n; i++) {
      if (i == 3) %OptimizeFunctionOnNextCall(migr);
      migr(objects[i], i);
    }

    // use again
    if (verbose) {
      print("-----------------------------");
      print("--- use 2");
      print();
    }
    var use = use_desc.use2 !== undefined ? use_desc.use2 : use_desc.use1;
    for (var i = 0; i < n; i++) {
      if (i == 3) %OptimizeFunctionOnNextCall(use);
      results[i] = use(objects[i], i);
      if (verbose >= 2) print(results[i]);
    }

  } catch (e) {
    if (verbose) print("--- incompatible use: " + e);
  }
  return results;
}


var ctors = [
  {
    name: "none-to-double",
    ctor: function(v) { return {a: v}; },
    args: function(i) { return [1.5 + i]; },
  },
  {
    name: "double",
    ctor: function(v) { var o = {}; o.a = v; return o; },
    args: function(i) { return [1.5 + i]; },
  },
  {
    name: "none-to-smi",
    ctor: function(v) { return {a: v}; },
    args: function(i) { return [i]; },
  },
  {
    name: "smi",
    ctor: function(v) { var o = {}; o.a = v; return o; },
    args: function(i) { return [i]; },
  },
  {
    name: "none-to-object",
    ctor: function(v) { return {a: v}; },
    args: function(i) { return ["s"]; },
  },
  {
    name: "object",
    ctor: function(v) { var o = {}; o.a = v; return o; },
    args: function(i) { return ["s"]; },
  },
  {
    name: "{a:, b:, c:}",
    ctor: function(v1, v2, v3) { return {a: v1, b: v2, c: v3}; },
    args: function(i)    { return [1.5 + i, 1.6, 1.7]; },
  },
  {
    name: "{a..h:}",
    ctor: function(v) { var o = {}; o.h=o.g=o.f=o.e=o.d=o.c=o.b=o.a=v; return o; },
    args: function(i) { return [1.5 + i]; },
  },
  {
    name: "1",
    ctor: function(v) { var o = 1; o.a = v; return o; },
    args: function(i) { return [1.5 + i]; },
  },
  {
    name: "f()",
    ctor: function(v) { var o = function() { return v;}; o.a = v; return o; },
    args: function(i) { return [1.5 + i]; },
  },
  {
    name: "f().bind",
    ctor: function(v) { var o = function(a,b,c) { return a+b+c; }; o = o.bind(o, v, v+1, v+2.2); return o; },
    args: function(i) { return [1.5 + i]; },
  },
  {
    name: "dictionary elements",
    ctor: function(v) { var o = []; o[1] = v; o[200000] = v; return o; },
    args: function(i) { return [1.5 + i]; },
  },
  {
    name: "json",
    ctor: function(v) { var json = '{"a":' + v + ',"b":' + v + '}'; return JSON.parse(json); },
    args: function(i) { return [1.5 + i]; },
  },
  {
    name: "fast accessors",
    accessor: {
        get: function() { return this.a_; },
        set: function(value) {this.a_ = value; },
        configurable: true,
    },
    ctor: function(v) {
      var o = {a_:v};
      Object.defineProperty(o, "a", this.accessor);
      return o;
    },
    args: function(i) { return [1.5 + i]; },
  },
  {
    name: "slow accessor",
    accessor1: { value: this.a_, configurable: true },
    accessor2: {
        get: function() { return this.a_; },
        set: function(value) {this.a_ = value; },
        configurable: true,
    },
    ctor: function(v) {
      var o = {a_:v};
      Object.defineProperty(o, "a", this.accessor1);
      Object.defineProperty(o, "a", this.accessor2);
      return o;
    },
    args: function(i) { return [1.5 + i]; },
  },
  {
    name: "slow",
    proto: {},
    ctor: function(v) {
      var o = {__proto__: this.proto};
      o.a = v;
      for (var i = 0; %HasFastProperties(o); i++) o["f"+i] = v;
      return o;
    },
    args: function(i) { return [1.5 + i]; },
  },
  {
    name: "global",
    ctor: function(v) { return global; },
    args: function(i) { return [i]; },
  },
];



var uses = [
  {
    name: "o.a+1.0",
    use1: function(o, i) { return o.a + 1.0; },
    use2: function(o, i) { return o.a + 1.1; },
  },
  {
    name: "o.b+1.0",
    use1: function(o, i) { return o.b + 1.0; },
    use2: function(o, i) { return o.b + 1.1; },
  },
  {
    name: "o[1]+1.0",
    use1: function(o, i) { return o[1] + 1.0; },
    use2: function(o, i) { return o[1] + 1.1; },
  },
  {
    name: "o[-1]+1.0",
    use1: function(o, i) { return o[-1] + 1.0; },
    use2: function(o, i) { return o[-1] + 1.1; },
  },
  {
    name: "()",
    use1: function(o, i) { return o() + 1.0; },
    use2: function(o, i) { return o() + 1.1; },
  },
];



var migrations = [
  {
    name: "to smi",
    migr: function(o, i) { if (i == 0) o.a = 1; },
  },
  {
    name: "to double",
    migr: function(o, i) { if (i == 0) o.a = 1.1; },
  },
  {
    name: "to object",
    migr: function(o, i) { if (i == 0) o.a = {}; },
  },
  {
    name: "set prototype {}",
    migr: function(o, i) { o.__proto__ = {}; },
  },
  {
    name: "%FunctionSetPrototype",
    migr: function(o, i) { %FunctionSetPrototype(o, null); },
  },
  {
    name: "modify prototype",
    migr: function(o, i) { if (i == 0) o.__proto__.__proto1__ = [,,,5,,,]; },
  },
  {
    name: "freeze prototype",
    migr: function(o, i) { if (i == 0) Object.freeze(o.__proto__); },
  },
  {
    name: "delete and re-add property",
    migr: function(o, i) { var v = o.a; delete o.a; o.a = v; },
  },
  {
    name: "modify prototype",
    migr: function(o, i) { if (i >= 0) o.__proto__ = {}; },
  },
  {
    name: "set property callback",
    migr: function(o, i) {
      Object.defineProperty(o, "a", {
        get: function() { return 1.5 + i; },
        set: function(value) {},
        configurable: true,
      });
    },
  },
  {
    name: "observe",
    migr: function(o, i) { Object.observe(o, function(){}); },
  },
  {
    name: "%EnableAccessChecks",
    migr: function(o, i) {
      if (typeof (o) !== 'function') %EnableAccessChecks(o);
    },
  },
  {
    name: "%DisableAccessChecks",
    migr: function(o, i) {
      if ((typeof (o) !== 'function') && (o !== global)) %DisableAccessChecks(o);
    },
  },
  {
    name: "seal",
    migr: function(o, i) { Object.seal(o); },
  },
  { // Must be the last in the sequence, because after the global object freeze
    // the other modifications does not make sence.
    name: "freeze",
    migr: function(o, i) { Object.freeze(o); },
  },
];



migrations.forEach(function(migr) {
  uses.forEach(function(use) {
    ctors.forEach(function(ctor) {
      test(ctor, use, migr);
    });
  });
});
