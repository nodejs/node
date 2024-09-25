// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/assertions.js
// META: script=/wasm/jsapi/wasm-module-builder.js
// META: script=/wasm/jsapi/js-string/polyfill.js

// Generate two sets of exports, one from a polyfill implementation and another
// from the builtins provided by the host.
let polyfillExports;
let builtinExports;
setup(() => {
  // Compile a module that exports a function for each builtin that will call
  // it. We could just generate a module that re-exports the builtins, but that
  // would not catch any special codegen that could happen when direct calling
  // a known builtin function from wasm.
  const builder = new WasmModuleBuilder();
  const arrayIndex = builder.addArray(kWasmI16, true, kNoSuperType, true);
  const builtins = [
    {
      name: "test",
      params: [kWasmExternRef],
      results: [kWasmI32],
    },
    {
      name: "cast",
      params: [kWasmExternRef],
      results: [wasmRefType(kWasmExternRef)],
    },
    {
      name: "fromCharCodeArray",
      params: [wasmRefNullType(arrayIndex), kWasmI32, kWasmI32],
      results: [wasmRefType(kWasmExternRef)],
    },
    {
      name: "intoCharCodeArray",
      params: [kWasmExternRef, wasmRefNullType(arrayIndex), kWasmI32],
      results: [kWasmI32],
    },
    {
      name: "fromCharCode",
      params: [kWasmI32],
      results: [wasmRefType(kWasmExternRef)],
    },
    {
      name: "fromCodePoint",
      params: [kWasmI32],
      results: [wasmRefType(kWasmExternRef)],
    },
    {
      name: "charCodeAt",
      params: [kWasmExternRef, kWasmI32],
      results: [kWasmI32],
    },
    {
      name: "codePointAt",
      params: [kWasmExternRef, kWasmI32],
      results: [kWasmI32],
    },
    {
      name: "length",
      params: [kWasmExternRef],
      results: [kWasmI32],
    },
    {
      name: "concat",
      params: [kWasmExternRef, kWasmExternRef],
      results: [wasmRefType(kWasmExternRef)],
    },
    {
      name: "substring",
      params: [kWasmExternRef, kWasmI32, kWasmI32],
      results: [wasmRefType(kWasmExternRef)],
    },
    {
      name: "equals",
      params: [kWasmExternRef, kWasmExternRef],
      results: [kWasmI32],
    },
    {
      name: "compare",
      params: [kWasmExternRef, kWasmExternRef],
      results: [kWasmI32],
    },
  ];

  // Add a function type for each builtin
  for (let builtin of builtins) {
    builtin.type = builder.addType({
      params: builtin.params,
      results: builtin.results
    });
  }

  // Add an import for each builtin
  for (let builtin of builtins) {
    builtin.importFuncIndex = builder.addImport(
      "wasm:js-string",
      builtin.name,
      builtin.type);
  }

  // Generate an exported function to call the builtin
  for (let builtin of builtins) {
    let func = builder.addFunction(builtin.name + "Imp", builtin.type);
    func.addLocals(builtin.params.length);
    let body = [];
    for (let i = 0; i < builtin.params.length; i++) {
      body.push(kExprLocalGet);
      body.push(...wasmSignedLeb(i));
    }
    body.push(kExprCallFunction);
    body.push(...wasmSignedLeb(builtin.importFuncIndex));
    func.addBody(body);
    func.exportAs(builtin.name);
  }

  const buffer = builder.toBuffer();

  // Instantiate this module using the builtins from the host
  const builtinModule = new WebAssembly.Module(buffer, {
    builtins: ["js-string"]
  });
  const builtinInstance = new WebAssembly.Instance(builtinModule, {});
  builtinExports = builtinInstance.exports;

  // Instantiate this module using the polyfill module
  const polyfillModule = new WebAssembly.Module(buffer);
  const polyfillInstance = new WebAssembly.Instance(polyfillModule, {
    "wasm:js-string": polyfillImports
  });
  polyfillExports = polyfillInstance.exports;
});

// A helper function to assert that the behavior of two functions are the
// same.
function assert_same_behavior(funcA, funcB, ...params) {
  let resultA;
  let errA = null;
  try {
    resultA = funcA(...params);
  } catch (err) {
    errA = err;
  }

  let resultB;
  let errB = null;
  try {
    resultB = funcB(...params);
  } catch (err) {
    errB = err;
  }

  if (errA || errB) {
    assert_equals(errA === null, errB === null, errA ? errA.message : errB.message);
    assert_equals(Object.getPrototypeOf(errA), Object.getPrototypeOf(errB));
  }
  assert_equals(resultA, resultB);

  if (errA) {
    throw errA;
  }
  return resultA;
}

function assert_throws_if(func, shouldThrow, constructor) {
  let error = null;
  try {
    func();
  } catch (e) {
    error = e;
  }
  assert_equals(error !== null, shouldThrow, "shouldThrow mismatch");
  if (shouldThrow && error !== null) {
    assert_true(error instanceof constructor);
  }
}

// Constant values used in the tests below
const testStrings = [
  "",
  "a",
  "1",
  "ab",
  "hello, world",
  "\n",
  "☺",
  "☺☺",
  String.fromCodePoint(0x10000, 0x10001)
];
const testCharCodes = [1, 2, 3, 10, 0x7f, 0xff, 0xfffe, 0xffff];
const testCodePoints = [1, 2, 3, 10, 0x7f, 0xff, 0xfffe, 0xffff, 0x10000, 0x10001];
const testExternRefValues = [
  null,
  undefined,
  true,
  false,
  {x:1337},
  ["abracadabra"],
  13.37,
  -0,
  0x7fffffff + 0.1,
  -0x7fffffff - 0.1,
  0x80000000 + 0.1,
  -0x80000000 - 0.1,
  0xffffffff + 0.1,
  -0xffffffff - 0.1,
  Number.EPSILON,
  Number.MAX_SAFE_INTEGER,
  Number.MIN_SAFE_INTEGER,
  Number.MIN_VALUE,
  Number.MAX_VALUE,
  Number.NaN,
  "hi",
  37n,
  new Number(42),
  new Boolean(true),
  Symbol("status"),
  () => 1337,
];

// Test that `test` and `cast` work on various JS values. Run all the
// other builtins and assert that they also perform equivalent type
// checks.
test(() => {
  for (let a of testExternRefValues) {
    let isString = assert_same_behavior(
      builtinExports['test'],
      polyfillExports['test'],
      a
    );

    assert_throws_if(() => assert_same_behavior(
        builtinExports['cast'],
        polyfillExports['cast'],
        a
      ), !isString, WebAssembly.RuntimeError);

    let arrayMutI16 = helperExports.createArrayMutI16(10);
    assert_throws_if(() => assert_same_behavior(
        builtinExports['intoCharCodeArray'],
        polyfillExports['intoCharCodeArray'],
        a, arrayMutI16, 0
      ), !isString, WebAssembly.RuntimeError);

    assert_throws_if(() => assert_same_behavior(
        builtinExports['charCodeAt'],
        polyfillExports['charCodeAt'],
        a, 0
      ), !isString, WebAssembly.RuntimeError);

    assert_throws_if(() => assert_same_behavior(
        builtinExports['codePointAt'],
        polyfillExports['codePointAt'],
        a, 0
      ), !isString, WebAssembly.RuntimeError);

    assert_throws_if(() => assert_same_behavior(
        builtinExports['length'],
        polyfillExports['length'],
        a
      ), !isString, WebAssembly.RuntimeError);

    assert_throws_if(() => assert_same_behavior(
        builtinExports['concat'],
        polyfillExports['concat'],
        a, a
      ), !isString, WebAssembly.RuntimeError);

    assert_throws_if(() => assert_same_behavior(
        builtinExports['substring'],
        polyfillExports['substring'],
        a, 0, 0
      ), !isString, WebAssembly.RuntimeError);

    assert_throws_if(() => assert_same_behavior(
        builtinExports['equals'],
        polyfillExports['equals'],
        a, a
      ), a !== null && !isString, WebAssembly.RuntimeError);

    assert_throws_if(() => assert_same_behavior(
        builtinExports['compare'],
        polyfillExports['compare'],
        a, a
      ), !isString, WebAssembly.RuntimeError);
  }
});

// Test that `fromCharCode` works on various char codes
test(() => {
  for (let a of testCharCodes) {
    assert_same_behavior(
      builtinExports['fromCharCode'],
      polyfillExports['fromCharCode'],
      a
    );
  }
});

// Test that `fromCodePoint` works on various code points
test(() => {
  for (let a of testCodePoints) {
    assert_same_behavior(
      builtinExports['fromCodePoint'],
      polyfillExports['fromCodePoint'],
      a
    );
  }
});

// Perform tests on various strings
test(() => {
  for (let a of testStrings) {
    let length = assert_same_behavior(
      builtinExports['length'],
      polyfillExports['length'],
      a
    );

    for (let i = 0; i < length; i++) {
      let charCode = assert_same_behavior(
        builtinExports['charCodeAt'],
        polyfillExports['charCodeAt'],
        a, i
      );
    }

    for (let i = 0; i < length; i++) {
      let charCode = assert_same_behavior(
        builtinExports['codePointAt'],
        polyfillExports['codePointAt'],
        a, i
      );
    }

    let arrayMutI16 = helperExports.createArrayMutI16(length);
    assert_same_behavior(
      builtinExports['intoCharCodeArray'],
      polyfillExports['intoCharCodeArray'],
      a, arrayMutI16, 0
    );

    assert_same_behavior(
      builtinExports['fromCharCodeArray'],
      polyfillExports['fromCharCodeArray'],
      arrayMutI16, 0, length
    );

    for (let i = 0; i < length; i++) {
      for (let j = 0; j < length; j++) {
        assert_same_behavior(
          builtinExports['substring'],
          polyfillExports['substring'],
          a, i, j
        );
      }
    }
  }
});

// Test various binary operations
test(() => {
  for (let a of testStrings) {
    for (let b of testStrings) {
      assert_same_behavior(
        builtinExports['concat'],
        polyfillExports['concat'],
        a, b
      );

      assert_same_behavior(
        builtinExports['equals'],
        polyfillExports['equals'],
        a, b
      );

      assert_same_behavior(
        builtinExports['compare'],
        polyfillExports['compare'],
        a, b
      );
    }
  }
});
