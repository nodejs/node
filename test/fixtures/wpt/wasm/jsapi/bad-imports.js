/**
 * `t` should be a function that takes at least three arguments:
 *
 * - the name of the test;
 * - the expected error (to be passed to `assert_throws_js`);
 * - a function that takes a `WasmModuleBuilder` and initializes it;
 * - (optionally) an options object.
 *
 * The function is expected to create a test that checks if instantiating a
 * module with the result of the `WasmModuleBuilder` and the options object
 * (if any) yields the correct error.
 */
function test_bad_imports(t) {
  function value_type(type) {
    switch (type) {
      case "i32": return kWasmI32;
      case "i64": return kWasmI64;
      case "f32": return kWasmF32;
      case "f64": return kWasmF64;
      default: throw new TypeError(`Unexpected type ${type}`);
    }
  }

  for (const value of [null, true, "", Symbol(), 1, 0.1, NaN]) {
    t(`Non-object imports argument: ${format_value(value)}`,
      TypeError,
      builder => {},
      value);
  }

  for (const value of [undefined, null, true, "", Symbol(), 1, 0.1, NaN]) {
    const imports = {
      "module": value,
    };
    t(`Non-object module: ${format_value(value)}`,
      TypeError,
      builder => {
        builder.addImport("module", "fn", kSig_v_v);
      },
      imports);
  }

  t(`Missing imports argument`,
    TypeError,
    builder => {
      builder.addImport("module", "fn", kSig_v_v);
    });

  for (const [value, name] of [[undefined, "undefined"], [{}, "empty object"], [{ "module\0": null }, "wrong property"]]) {
    t(`Imports argument with missing property: ${name}`,
      TypeError,
      builder => {
        builder.addImport("module", "fn", kSig_v_v);
      },
      value);
  }

  for (const value of [undefined, null, true, "", Symbol(), 1, 0.1, NaN, {}]) {
    t(`Importing a function with an incorrectly-typed value: ${format_value(value)}`,
      WebAssembly.LinkError,
      builder => {
        builder.addImport("module", "fn", kSig_v_v);
      },
      {
        "module": {
          "fn": value,
        },
      });
  }

  const nonGlobals = [
    [undefined],
    [null],
    [true],
    [""],
    [Symbol()],
    [{}, "plain object"],
    [WebAssembly.Global, "WebAssembly.Global"],
    [WebAssembly.Global.prototype, "WebAssembly.Global.prototype"],
    [Object.create(WebAssembly.Global.prototype), "Object.create(WebAssembly.Global.prototype)"],
  ];

  for (const type of ["i32", "i64", "f32", "f64"]) {
    const extendedNonGlobals = nonGlobals.concat([
      type === "i64" ? [0, "Number"] : [0n, "BigInt"],
      [new WebAssembly.Global({value: type === "f32" ? "f64" : "f32"}), "WebAssembly.Global object (wrong value type)"],
    ]);
    for (const [value, name = format_value(value)] of extendedNonGlobals) {
      t(`Importing an ${type} global with an incorrectly-typed value: ${name}`,
        WebAssembly.LinkError,
        builder => {
          builder.addImportedGlobal("module", "global", value_type(type));
        },
        {
          "module": {
            "global": value,
          },
        });
    }
  }

  for (const type of ["i32", "i64", "f32", "f64"]) {
    const value = type === "i64" ? 0n : 0;
    t(`Importing an ${type} mutable global with a primitive value`,
      WebAssembly.LinkError,
      builder => {
        builder.addImportedGlobal("module", "global", value_type(type), true);
      },
      {
        "module": {
          "global": value,
        },
      });

    const global = new WebAssembly.Global({ "value": type }, value);
    t(`Importing an ${type} mutable global with an immutable Global object`,
      WebAssembly.LinkError,
      builder => {
        builder.addImportedGlobal("module", "global", value_type(type), true);
      },
      {
        "module": {
          "global": global,
        },
      });
  }

  const nonMemories = [
    [undefined],
    [null],
    [true],
    [""],
    [Symbol()],
    [1],
    [0.1],
    [NaN],
    [{}, "plain object"],
    [WebAssembly.Memory, "WebAssembly.Memory"],
    [WebAssembly.Memory.prototype, "WebAssembly.Memory.prototype"],
    [Object.create(WebAssembly.Memory.prototype), "Object.create(WebAssembly.Memory.prototype)"],
    [new WebAssembly.Memory({"initial": 256}), "WebAssembly.Memory object (too large)"],
  ];

  for (const [value, name = format_value(value)] of nonMemories) {
    t(`Importing memory with an incorrectly-typed value: ${name}`,
      WebAssembly.LinkError,
      builder => {
        builder.addImportedMemory("module", "memory", 0, 128);
      },
      {
        "module": {
          "memory": value,
        },
      });
  }

  const nonTables = [
    [undefined],
    [null],
    [true],
    [""],
    [Symbol()],
    [1],
    [0.1],
    [NaN],
    [{}, "plain object"],
    [WebAssembly.Table, "WebAssembly.Table"],
    [WebAssembly.Table.prototype, "WebAssembly.Table.prototype"],
    [Object.create(WebAssembly.Table.prototype), "Object.create(WebAssembly.Table.prototype)"],
    [new WebAssembly.Table({"element": "anyfunc", "initial": 256}), "WebAssembly.Table object (too large)"],
  ];

  for (const [value, name = format_value(value)] of nonTables) {
    t(`Importing table with an incorrectly-typed value: ${name}`,
      WebAssembly.LinkError,
      builder => {
        builder.addImportedTable("module", "table", 0, 128);
      },
      {
        "module": {
          "table": value,
        },
      });
  }
}
globalThis.test_bad_imports = test_bad_imports;
