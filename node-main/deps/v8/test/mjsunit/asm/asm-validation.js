// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

// Note that this test file contains tests that explicitly check modules are
// valid asm.js and then break them with invalid instantiation arguments.

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
  assertFalse(%IsAsmWasmCode(DisallowAssignToConstGlobal));
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
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(123, m.foo());
})();

(function TestBadArgTypes() {
  function Module(a, b, c) {
    "use asm";
    var NaN = a.NaN;
    return {};
  }
  var m = Module(1, 2, 3);
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals({}, m);
})();

(function TestBadArgTypesMismatch() {
  function Module(a, b, c) {
    "use asm";
    var NaN = a.NaN;
    return {};
  }
  var m = Module(1, 2);
  assertFalse(%IsAsmWasmCode(Module));
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
  assertFalse(%IsAsmWasmCode(Module));
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
  assertFalse(%IsAsmWasmCode(Module));
  var m2 = Module(1, 2, 3);
  assertFalse(%IsAsmWasmCode(Module));
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
  assertFalse(%IsAsmWasmCode(Module1));
  var m2 = Module2({}, {}, heap);
  assertFalse(%IsAsmWasmCode(Module2));
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
  assertFalse(%IsAsmWasmCode(Module2));
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
  assertFalse(%IsAsmWasmCode(Module2));
  var m1b = Module1({NaN: NaN}, {}, heap);
  assertFalse(%IsAsmWasmCode(Module1));
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
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(0xffffffff, m.foo());
})();

(function TestBadBooleanParamAnnotation() {
  function Module() {
    "use asm";
    function foo(x) {
      x = x | true;
      return x;
    }
    return { foo: foo };
  }
  var m = Module();
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(3, m.foo(3));
})();

(function TestBadExportTwice() {
  function Module() {
    "use asm";
    function bar() { return 1; }
    function baz() { return 2; }
    return {foo: bar, foo: baz};
  }
  var m = Module();
  assertTrue(%IsAsmWasmCode(Module));
  assertEquals(2, m.foo());
})();

(function TestBadImport() {
  function Module(stdlib) {
    "use asm";
    var set = 0;
    var foo = stdlib[set];
    return {};
  }
  var m = Module(this);
  assertFalse(%IsAsmWasmCode(Module));
})();

(function TestBadFroundTrue() {
  function Module(stdlib) {
    "use asm";
    var fround = stdlib.Math.fround;
    function foo() {
      var x = fround(true);
      return +x;
    }
    return { foo: foo };
  }
  var m = Module(this);
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(1, m.foo());
})();

(function TestBadCase() {
  function Module() {
    "use asm";
    function foo(x) {
      x = x | 0;
      switch (x|0) {
        case true:
          return 42;
        default:
          return 43;
      }
      return 0;
    }
    return { foo: foo };
  }
  var m = Module();
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(43, m.foo(3));
})();

(function TestVarHidesExport() {
  function Module() {
    "use asm";
    var foo;
    function foo() {}
    return foo;
  }
  Module();
  assertFalse(%IsAsmWasmCode(Module));
})();

(function TestUndefinedGlobalCall() {
  function Module() {
    "use asm";
    function foo() {
      return bar() | 0;
    }
    return foo;
  }
  Module();
  assertFalse(%IsAsmWasmCode(Module));
})();

(function TestConditionalReturn() {
  function Module() {
    'use asm';
    function foo(a, b) {
      a = +a;
      b = +b;
      // Allowed, despite not matching the spec, as emscripten emits this in
      // practice.
      return a == b ? +a : +b;
    }
    return foo;
  }
  var m = Module();
  assertEquals(4, m(4, 4));
  assertEquals(5, m(4, 5));
  assertEquals(4, m(5, 4));
  assertValidAsm(Module);
})();

(function TestMismatchedConditionalReturn() {
  function Module() {
    'use asm';
    function foo(a, b) {
      a = +a;
      return a == 0.0 ? 0 : +a;
    }
    return foo;
  }
  Module();
  assertFalse(%IsAsmWasmCode(Module));
})();

(function TestBadIntConditionalReturn() {
  function Module() {
    'use asm';
    function foo(a, b) {
      a = a | 0;
      b = b | 0;
      // Disallowed because signature must be signed, but these will be int.
      return 1 ? a : b;
    }
    return foo;
  }
  Module();
  assertFalse(%IsAsmWasmCode(Module));
})();

(function TestBadSignedConditionalReturn() {
  function Module() {
    'use asm';
    function foo(a, b) {
      a = a | 0;
      b = b | 0;
      // Disallowed because conditional yields int, even when both sides
      // are signed.
      return 1 ? a | 0 : b | 0;
    }
    return foo;
  }
  Module();
  assertFalse(%IsAsmWasmCode(Module));
})();

(function TestAsmIsRegular() {
  function Module() {
    'use asm';
    var g = 123;
    function foo() {
      return g | 0;
    }
    return {x: foo};
  }
  var o = Module();
  assertValidAsm(Module);
  assertFalse(o instanceof WebAssembly.Instance);
  assertTrue(o instanceof Object);
  assertTrue(o.__proto__ === Object.prototype);
  var p = Object.getOwnPropertyDescriptor(o, "x")
  assertTrue(p.writable);
  assertTrue(p.enumerable);
  assertTrue(p.configurable);
  assertTrue(typeof o.x === 'function');
  o.x = 5;
  assertTrue(typeof o.x === 'number');
  assertTrue(o.__single_function__ === undefined);
  assertTrue(o.__foreign_init__ === undefined);
})();

(function TestAsmExportOrderPreserved() {
  function Module() {
    "use asm";
    function f() {}
    function g() {}
    return { a:f, b:g, x:f, c:g, d:f };
  }
  var m = Module();
  assertValidAsm(Module);
  var props = Object.getOwnPropertyNames(m);
  assertEquals(["a","b","x","c","d"], props);
})();

(function TestDuplicateParameterName() {
  function module1(x, x, heap) {
    'use asm';
    return {};
  }
  module1({}, {}, new ArrayBuffer(4096));
  assertFalse(%IsAsmWasmCode(module1));

  function module2(x, ffi, x) {
    'use asm';
    return {};
  }
  module2({}, {}, new ArrayBuffer(4096));
  assertFalse(%IsAsmWasmCode(module2));

  function module3(stdlib, x, x) {
    'use asm';
    return {};
  }
  module3({}, {}, new ArrayBuffer(4096));
  assertFalse(%IsAsmWasmCode(module3));

  // Regression test for https://crbug.com/1068355.
  function regress1068355(ffi, ffi, heap) {
    'use asm';
    var result = new ffi.Uint8Array(heap);
    function bar() {}
    return {f: bar};
  }
  let heap = new ArrayBuffer(4096);
  assertThrows(
      () => regress1068355({Uint8Array: Uint8Array}, {}, heap), TypeError,
      /Uint8Array is not a constructor/);
  assertFalse(%IsAsmWasmCode(regress1068355));
})();

(function TestTooManyParametersToImport() {
  function MakeModule(num_arguments) {
    let template = `
      'use asm';
      var imported = foreign.imported;
      function main() {
        imported(ARGS);
      }

      return main;
      `;
    let args = new Array(num_arguments).fill('0').join(', ');
    return new Function('stdlib', 'foreign', template.replace('ARGS', args));
  }

  // V8 has an internal limit of 1000 parameters (see wasm-limits.h).
  let Module1000Params = MakeModule(1000);
  let Module1001Params = MakeModule(1001);

  Module1000Params({}, {imported: i => i});
  Module1001Params({}, {imported: i => i});

  assertTrue(%IsAsmWasmCode(Module1000Params));
  assertFalse(%IsAsmWasmCode(Module1001Params));
})();
