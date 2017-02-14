// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

function assertValidAsm(func) {
  assertTrue(%IsAsmWasmCode(func));
}

(function TestConst() {
  function Module(s) {
    "use asm";
    var fround = s.Math.fround;
    // Global constants. These are treated just like numeric literals.
    const fConst = fround(-3.0);
    const dConst = -3.0;
    const iConst = -3;

    // consts can be used to initialize other consts.
    const fPrime = fConst;

    // The following methods verify that return statements with global constants
    // do not need type annotations.
    function f() {
      return fPrime;
    }
    function d() {
      return dConst;
    }
    function i() {
      return iConst;
    }

    // The following methods verify that locals initialized with global
    // constants do not need type annotations.
    function fVar() {
      var v = fPrime;
      return fround(v);
    }
    function iVar() {
      var v = iConst;
      return v|0;
    }
    function dVar() {
      var v = dConst;
      return +v;
    }

    return {
      f: f, d: d, i: i,
      fVar: fVar, dVar: dVar, iVar: iVar,
    };
  }

  function DisallowAssignToConstGlobal() {
    const constant = 0;
    function invalid(i) {
      i = i|0;
      constant = i;
      return constant;
    }
    return invalid;
  }

  var m = Module(this);
  assertValidAsm(Module);

  assertEquals(-3, m.i());
  assertEquals(-3.0, m.d());
  assertEquals(Math.fround(-3.0), m.f());

  assertEquals(-3, m.iVar());
  assertEquals(-3.0, m.dVar());
  assertEquals(Math.fround(-3.0), m.fVar());

  var m = DisallowAssignToConstGlobal();
  assertTrue(%IsNotAsmWasmCode(DisallowAssignToConstGlobal));
})();

(function TestModuleArgs() {
  function Module1(stdlib) {
    "use asm";
    function foo() { }
    return { foo: foo };
  }
  function Module2(stdlib, ffi) {
    "use asm";
    function foo() { }
    return { foo: foo };
  }
  function Module3(stdlib, ffi, heap) {
    "use asm";
    function foo() { }
    return { foo: foo };
  }
  var modules = [Module1, Module2, Module3];
  var heap = new ArrayBuffer(1024 * 1024);
  for (var i = 0; i < modules.length; ++i) {
    print('Module' + (i + 1));
    var module = modules[i];
    var m = module();
    assertValidAsm(module);
    var m = module({});
    assertValidAsm(module);
    var m = module({}, {});
    assertValidAsm(module);
    var m = module({}, {}, heap);
    assertValidAsm(module);
    var m = module({}, {}, heap, {});
    assertValidAsm(module);
  }
})();

(function TestBadModule() {
  function Module(stdlib, ffi, heap) {
    "use asm";
    function foo() { var y = 3; var x = 1 + y; return 123; }
    return { foo: foo };
  }
  var m = Module({});
  assertTrue(%IsNotAsmWasmCode(Module));
  assertEquals(123, m.foo());
})();

(function TestBadArgTypes() {
  function Module(a, b, c) {
    "use asm";
    var NaN = a.NaN;
    return {};
  }
  var m = Module(1, 2, 3);
  assertTrue(%IsNotAsmWasmCode(Module));
  assertEquals({}, m);
})();

(function TestBadArgTypesMismatch() {
  function Module(a, b, c) {
    "use asm";
    var NaN = a.NaN;
    return {};
  }
  var m = Module(1, 2);
  assertTrue(%IsNotAsmWasmCode(Module));
  assertEquals({}, m);
})();

(function TestModuleNoStdlib() {
  function Module() {
    "use asm";
    function foo() { return 123; }
    return { foo: foo };
  }
  var m = Module({});
  assertValidAsm(Module);
  assertEquals(123, m.foo());
})();

(function TestModuleWith5() {
  function Module(a, b, c, d, e) {
    "use asm";
    function foo() { return 123; }
    return { foo: foo };
  }
  var heap = new ArrayBuffer(1024 * 1024);
  var m = Module({}, {}, heap);
  assertTrue(%IsNotAsmWasmCode(Module));
  assertEquals(123, m.foo());
})();

(function TestModuleNoStdlibCall() {
  function Module(stdlib, ffi, heap) {
    "use asm";
    function foo() { return 123; }
    return { foo: foo };
  }
  var m = Module();
  assertValidAsm(Module);
  assertEquals(123, m.foo());
})();

(function TestModuleNew() {
  function Module(stdlib, ffi, heap) {
    "use asm";
    function foo() { return 123; }
    return { foo: foo };
  }
  var m = new Module({}, {});
  assertValidAsm(Module);
  assertEquals(123, m.foo());
})();

(function TestMultipleFailures() {
  function Module(stdlib) {
    "use asm";
    var NaN = stdlib.NaN;
    function foo() { return 123; }
    return { foo: foo };
  }
  var m1 = Module(1, 2, 3);
  assertTrue(%IsNotAsmWasmCode(Module));
  var m2 = Module(1, 2, 3);
  assertTrue(%IsNotAsmWasmCode(Module));
  assertEquals(123, m1.foo());
  assertEquals(123, m2.foo());
})();

(function TestFailureThenSuccess() {
  function MkModule() {
    function Module(stdlib, ffi, heap) {
      "use asm";
      var NaN = stdlib.NaN;
      function foo() { return 123; }
      return { foo: foo };
    }
    return Module;
  }
  var Module1 = MkModule();
  var Module2 = MkModule();
  var heap = new ArrayBuffer(1024 * 1024);
  var m1 = Module1(1, 2, 3);
  assertTrue(%IsNotAsmWasmCode(Module1));
  var m2 = Module2({}, {}, heap);
  assertTrue(%IsNotAsmWasmCode(Module2));
  assertEquals(123, m1.foo());
  assertEquals(123, m2.foo());
})();

(function TestSuccessThenFailure() {
  function MkModule() {
    function Module(stdlib, ffi, heap) {
      "use asm";
      var NaN = stdlib.NaN;
      function foo() { return 123; }
      return { foo: foo };
    }
    return Module;
  }
  var Module1 = MkModule();
  var Module2 = MkModule();
  var heap = new ArrayBuffer(1024 * 1024);
  var m1 = Module1({NaN: NaN}, {}, heap);
  assertValidAsm(Module1);
  var m2 = Module2(1, 2, 3);
  assertTrue(%IsNotAsmWasmCode(Module2));
  assertEquals(123, m1.foo());
  assertEquals(123, m2.foo());
})();

(function TestSuccessThenFailureThenRetry() {
  function MkModule() {
    function Module(stdlib, ffi, heap) {
      "use asm";
      var NaN = stdlib.NaN;
      function foo() { return 123; }
      return { foo: foo };
    }
    return Module;
  }
  var Module1 = MkModule();
  var Module2 = MkModule();
  var heap = new ArrayBuffer(1024 * 1024);
  var m1a = Module1({NaN: NaN}, {}, heap);
  assertValidAsm(Module1);
  var m2 = Module2(1, 2, 3);
  assertTrue(%IsNotAsmWasmCode(Module2));
  var m1b = Module1({NaN: NaN}, {}, heap);
  assertTrue(%IsNotAsmWasmCode(Module1));
  assertEquals(123, m1a.foo());
  assertEquals(123, m1b.foo());
  assertEquals(123, m2.foo());
})();

(function TestBoundFunction() {
  function Module(stdlib, ffi, heap) {
    "use asm";
    function foo() { return 123; }
    return { foo: foo };
  }
  var heap = new ArrayBuffer(1024 * 1024);
  var ModuleBound = Module.bind(this, {}, {}, heap);
  var m = ModuleBound();
  assertValidAsm(Module);
  assertEquals(123, m.foo());
})();

(function TestBadConstUnsignedReturn() {
  function Module() {
    "use asm";
    const i = 0xffffffff;
    function foo() { return i; }
    return { foo: foo };
  }
  var m = Module();
  assertTrue(%IsNotAsmWasmCode(Module));
  assertEquals(0xffffffff, m.foo());
})();
