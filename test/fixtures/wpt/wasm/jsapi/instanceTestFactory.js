const instanceTestFactory = [
  [
    "Empty module without imports argument",
    function() {
      return {
        buffer: emptyModuleBinary,
        args: [],
        exports: {},
        verify: () => {},
      };
    }
  ],

  [
    "Empty module with undefined imports argument",
    function() {
      return {
        buffer: emptyModuleBinary,
        args: [undefined],
        exports: {},
        verify: () => {},
      };
    }
  ],

  [
    "Empty module with empty imports argument",
    function() {
      return {
        buffer: emptyModuleBinary,
        args: [{}],
        exports: {},
        verify: () => {},
      };
    }
  ],

  [
    "getter order for imports object",
    function() {
      const builder = new WasmModuleBuilder();
      builder.addImportedGlobal("module", "global1", kWasmI32);
      builder.addImportedGlobal("module2", "global3", kWasmI32);
      builder.addImportedMemory("module", "memory", 0, 128);
      builder.addImportedGlobal("module", "global2", kWasmI32);
      const buffer = builder.toBuffer();
      const order = [];

      const imports = {
        get module() {
          order.push("module getter");
          return {
            get global1() {
              order.push("global1 getter");
              return 0;
            },
            get global2() {
              order.push("global2 getter");
              return 0;
            },
            get memory() {
              order.push("memory getter");
              return new WebAssembly.Memory({ "initial": 64, maximum: 128 });
            },
          }
        },
        get module2() {
          order.push("module2 getter");
          return {
            get global3() {
              order.push("global3 getter");
              return 0;
            },
          }
        },
      };

      const expected = [
        "module getter",
        "global1 getter",
        "module2 getter",
        "global3 getter",
        "module getter",
        "memory getter",
        "module getter",
        "global2 getter",
      ];
      return {
        buffer,
        args: [imports],
        exports: {},
        verify: () => assert_array_equals(order, expected),
      };
    }
  ],

  [
    "imports",
    function() {
      const builder = new WasmModuleBuilder();

      builder.addImport("module", "fn", kSig_v_v);
      builder.addImportedGlobal("module", "global", kWasmI32);
      builder.addImportedMemory("module", "memory", 0, 128);
      builder.addImportedTable("module", "table", 0, 128);

      const buffer = builder.toBuffer();
      const imports = {
        "module": {
          "fn": function() {},
          "global": 0,
          "memory": new WebAssembly.Memory({ "initial": 64, maximum: 128 }),
          "table": new WebAssembly.Table({ "element": "anyfunc", "initial": 64, maximum: 128 }),
        },
        get "module2"() {
          assert_unreached("Should not get modules that are not imported");
        },
      };

      return {
        buffer,
        args: [imports],
        exports: {},
        verify: () => {},
      };
    }
  ],

  [
    "imports with empty module names",
    function() {
      const builder = new WasmModuleBuilder();

      builder.addImport("", "fn", kSig_v_v);
      builder.addImportedGlobal("", "global", kWasmI32);
      builder.addImportedMemory("", "memory", 0, 128);
      builder.addImportedTable("", "table", 0, 128);

      const buffer = builder.toBuffer();
      const imports = {
        "": {
          "fn": function() {},
          "global": 0,
          "memory": new WebAssembly.Memory({ "initial": 64, maximum: 128 }),
          "table": new WebAssembly.Table({ "element": "anyfunc", "initial": 64, maximum: 128 }),
        },
      };

      return {
        buffer,
        args: [imports],
        exports: {},
        verify: () => {},
      };
    }
  ],

  [
    "imports with empty names",
    function() {
      const builder = new WasmModuleBuilder();

      builder.addImport("a", "", kSig_v_v);
      builder.addImportedGlobal("b", "", kWasmI32);
      builder.addImportedMemory("c", "", 0, 128);
      builder.addImportedTable("d", "", 0, 128);

      const buffer = builder.toBuffer();
      const imports = {
        "a": { "": function() {} },
        "b": { "": 0 },
        "c": { "": new WebAssembly.Memory({ "initial": 64, maximum: 128 }) },
        "d": { "": new WebAssembly.Table({ "element": "anyfunc", "initial": 64, maximum: 128 }) },
      };

      return {
        buffer,
        args: [imports],
        exports: {},
        verify: () => {},
      };
    }
  ],

  [
    "exports with empty name: function",
    function() {
      const builder = new WasmModuleBuilder();

      builder
        .addFunction("", kSig_v_d)
        .addBody([])
        .exportFunc();

      const buffer = builder.toBuffer();

      const exports = {
        "": { "kind": "function", "name": "0", "length": 1 },
      };

      return {
        buffer,
        args: [],
        exports,
        verify: () => {},
      };
    }
  ],

  [
    "exports with empty name: table",
    function() {
      const builder = new WasmModuleBuilder();

      builder.setTableBounds(1);
      builder.addExportOfKind("", kExternalTable, 0);

      const buffer = builder.toBuffer();

      const exports = {
        "": { "kind": "table", "length": 1 },
      };

      return {
        buffer,
        args: [],
        exports,
        verify: () => {},
      };
    }
  ],

  [
    "exports with empty name: global",
    function() {
      const builder = new WasmModuleBuilder();

      builder.addGlobal(kWasmI32, true)
        .exportAs("")
        .init = 7;

      const buffer = builder.toBuffer();

      const exports = {
        "": { "kind": "global", "value": 7 },
      };

      return {
        buffer,
        args: [],
        exports,
        verify: () => {},
      };
    }
  ],

  [
    "No imports",
    function() {
      const builder = new WasmModuleBuilder();

      builder
        .addFunction("fn", kSig_v_d)
        .addBody([])
        .exportFunc();
      builder
        .addFunction("fn2", kSig_v_v)
        .addBody([])
        .exportFunc();

      builder.setTableBounds(1);
      builder.addExportOfKind("table", kExternalTable, 0);

      builder.addGlobal(kWasmI32, true)
        .exportAs("global")
        .init = 7;
      builder.addGlobal(kWasmF64, true)
        .exportAs("global2")
        .init = 1.2;

      builder.addMemory(4, 8, true);

      const buffer = builder.toBuffer();

      const exports = {
        "fn": { "kind": "function", "name": "0", "length": 1 },
        "fn2": { "kind": "function", "name": "1", "length": 0 },
        "table": { "kind": "table", "length": 1 },
        "global": { "kind": "global", "value": 7 },
        "global2": { "kind": "global", "value": 1.2 },
        "memory": { "kind": "memory", "size": 4 },
      };

      return {
        buffer,
        args: [],
        exports,
        verify: () => {},
      };
    }
  ],

  [
    "exports and imports",
    function() {
      const value = 102;

      const builder = new WasmModuleBuilder();

      const index = builder.addImportedGlobal("module", "global", kWasmI32);
      builder
        .addFunction("fn", kSig_i_v)
        .addBody([
            kExprGlobalGet,
            index,
            kExprReturn,
        ])
        .exportFunc();

      const buffer = builder.toBuffer();

      const imports = {
        "module": {
          "global": value,
        },
      };

      const exports = {
        "fn": { "kind": "function", "name": "0", "length": 0 },
      };

      return {
        buffer,
        args: [imports],
        exports,
        verify: instance => assert_equals(instance.exports.fn(), value)
      };
    }
  ],

  [
    "i64 exports and imports",
    function() {
      const value = 102n;

      const builder = new WasmModuleBuilder();

      const index = builder.addImportedGlobal("module", "global", kWasmI64);
      builder
        .addFunction("fn", kSig_l_v)
        .addBody([
            kExprGlobalGet,
            index,
            kExprReturn,
        ])
        .exportFunc();

      const index2 = builder.addImportedGlobal("module", "global2", kWasmI64);
      builder.addExportOfKind("global", kExternalGlobal, index2);

      const buffer = builder.toBuffer();

      const imports = {
        "module": {
          "global": value,
          "global2": 2n ** 63n,
        },
      };

      const exports = {
        "fn": { "kind": "function", "name": "0", "length": 0 },
        "global": { "kind": "global", "value": -(2n ** 63n) },
      };

      return {
        buffer,
        args: [imports],
        exports,
        verify: instance => assert_equals(instance.exports.fn(), value)
      };
    }
  ],

  [
    "import with i32-returning function",
    function() {
      const builder = new WasmModuleBuilder();

      const fnIndex = builder.addImport("module", "fn", kSig_i_v);
      const fn2 = builder
        .addFunction("fn2", kSig_v_v)
        .addBody([
            kExprCallFunction,
            fnIndex,
            kExprReturn,
        ])
        .exportFunc();

      const buffer = builder.toBuffer();

      let called = false;
      const imports = {
        "module": {
          "fn": function() {
            called = true;
            return 6n;
          },
        },
      };

      return {
        buffer,
        args: [imports],
        exports: {
          "fn2": { "kind": "function", "name": String(fn2.index), "length": 0 },
        },
        verify: instance => {
          assert_throws_js(TypeError, () => instance.exports.fn2());
          assert_true(called, "Should have called into JS");
        }
      };
    }
  ],

  [
    "import with function that takes and returns i32",
    function() {
      const builder = new WasmModuleBuilder();

      const fnIndex = builder.addImport("module", "fn", kSig_i_i);
      const fn2 = builder
        .addFunction("fn2", kSig_i_v)
        .addBody([
            kExprI32Const, 0x66,
            kExprCallFunction,
            fnIndex,
            kExprReturn,
        ])
        .exportFunc();

      const buffer = builder.toBuffer();

      let called = false;
      const imports = {
        "module": {
          "fn": function(n) {
            called = true;
            assert_equals(n, -26);
            return { valueOf() { return 6; } };
          },
        },
      };

      return {
        buffer,
        args: [imports],
        exports: {
          "fn2": { "kind": "function", "name": String(fn2.index), "length": 0 },
        },
        verify: instance => {
          assert_equals(instance.exports.fn2(), 6);
          assert_true(called, "Should have called into JS");
        }
      };
    }
  ],

  [
    "import with i64-returning function",
    function() {
      const builder = new WasmModuleBuilder();

      const fnIndex = builder.addImport("module", "fn", kSig_l_v);
      const fn2 = builder
        .addFunction("fn2", kSig_v_v)
        .addBody([
            kExprCallFunction,
            fnIndex,
            kExprReturn,
        ])
        .exportFunc();

      const buffer = builder.toBuffer();

      let called = false;
      const imports = {
        "module": {
          "fn": function() {
            called = true;
            return 6;
          },
        },
      };

      return {
        buffer,
        args: [imports],
        exports: {
          "fn2": { "kind": "function", "name": String(fn2.index), "length": 0 },
        },
        verify: instance => {
          assert_throws_js(TypeError, () => instance.exports.fn2());
          assert_true(called, "Should have called into JS");
        }
      };
    }
  ],

  [
    "import with function that takes and returns i64",
    function() {
      const builder = new WasmModuleBuilder();

      const fnIndex = builder.addImport("module", "fn", kSig_l_l);
      const fn2 = builder
        .addFunction("fn2", kSig_l_v)
        .addBody([
            kExprI64Const, 0x66,
            kExprCallFunction,
            fnIndex,
            kExprReturn,
        ])
        .exportFunc();

      const buffer = builder.toBuffer();

      let called = false;
      const imports = {
        "module": {
          "fn": function(n) {
            called = true;
            assert_equals(n, -26n);
            return { valueOf() { return 6n; } };
          },
        },
      };

      return {
        buffer,
        args: [imports],
        exports: {
          "fn2": { "kind": "function", "name": String(fn2.index), "length": 0 },
        },
        verify: instance => {
          assert_equals(instance.exports.fn2(), 6n);
          assert_true(called, "Should have called into JS");
        }
      };
    }
  ],

  [
    "import with i32-taking function",
    function() {
      const builder = new WasmModuleBuilder();

      const fn = builder
        .addFunction("fn", kSig_v_i)
        .addBody([
            kExprReturn,
        ])
        .exportFunc();

      const buffer = builder.toBuffer();

      return {
        buffer,
        args: [],
        exports: {
          "fn": { "kind": "function", "name": String(fn.index), "length": 1 },
        },
        verify: instance => assert_throws_js(TypeError, () => instance.exports.fn(6n))
      };
    }
  ],

  [
    "import with i64-taking function",
    function() {
      const builder = new WasmModuleBuilder();

      const fn = builder
        .addFunction("fn", kSig_v_l)
        .addBody([
            kExprReturn,
        ])
        .exportFunc();

      const buffer = builder.toBuffer();

      return {
        buffer,
        args: [],
        exports: {
          "fn": { "kind": "function", "name": String(fn.index), "length": 1 },
        },
        verify: instance => assert_throws_js(TypeError, () => instance.exports.fn(6))
      };
    }
  ],

  [
    "export i64-returning function",
    function() {
      const builder = new WasmModuleBuilder();

      const fn = builder
        .addFunction("fn", kSig_l_v)
        .addBody([
            kExprI64Const, 0x66,
            kExprReturn,
        ])
        .exportFunc();

      const buffer = builder.toBuffer();

      return {
        buffer,
        args: [],
        exports: {
          "fn": { "kind": "function", "name": String(fn.index), "length": 0 },
        },
        verify: instance => assert_equals(instance.exports.fn(), -26n)
      };
    }
  ],

  [
    "i32 mutable WebAssembly.Global import",
    function() {
      const initial = 102;
      const value = new WebAssembly.Global({ "value": "i32", "mutable": true }, initial);

      const builder = new WasmModuleBuilder();

      const index = builder.addImportedGlobal("module", "global", kWasmI32, true);
      const fn = builder
        .addFunction("fn", kSig_i_v)
        .addBody([
            kExprGlobalGet,
            index,
            kExprReturn,
        ])
        .exportFunc();

      const buffer = builder.toBuffer();

      const imports = {
        "module": {
          "global": value,
        },
      };

      const exports = {
        "fn": { "kind": "function", "name": String(fn.index), "length": 0 },
      };

      return {
        buffer,
        args: [imports],
        exports,
        verify: instance => {
          assert_equals(instance.exports.fn(), initial);
          const after = 201;
          value.value = after;
          assert_equals(instance.exports.fn(), after);
        }
      };
    }
  ],

  [
    "i64 mutable WebAssembly.Global import",
    function() {
      const initial = 102n;
      const value = new WebAssembly.Global({ "value": "i64", "mutable": true }, initial);

      const builder = new WasmModuleBuilder();

      const index = builder.addImportedGlobal("module", "global", kWasmI64, true);
      const fn = builder
        .addFunction("fn", kSig_l_v)
        .addBody([
            kExprGlobalGet,
            index,
            kExprReturn,
        ])
        .exportFunc();

      const buffer = builder.toBuffer();

      const imports = {
        "module": {
          "global": value,
        },
      };

      const exports = {
        "fn": { "kind": "function", "name": String(fn.index), "length": 0 },
      };

      return {
        buffer,
        args: [imports],
        exports,
        verify: instance => {
          assert_equals(instance.exports.fn(), initial);
          const after = 201n;
          value.value = after;
          assert_equals(instance.exports.fn(), after);
        }
      };
    }
  ],

  [
    "Multiple i64 arguments",
    function() {
      const builder = new WasmModuleBuilder();

      const fn = builder
          .addFunction("fn", kSig_l_ll)
          .addBody([
              kExprLocalGet, 1,
          ])
          .exportFunc();

      const buffer = builder.toBuffer();

      const exports = {
        "fn": { "kind": "function", "name": String(fn.index), "length": 2 },
      };

      return {
        buffer,
        args: [],
        exports,
        verify: instance => {
          const fn = instance.exports.fn;
          assert_equals(fn(1n, 0n), 0n);
          assert_equals(fn(1n, 123n), 123n);
          assert_equals(fn(1n, -123n), -123n);
          assert_equals(fn(1n, "5"), 5n);
          assert_throws_js(TypeError, () => fn(1n, 5));
        }
      };
    }
  ],

  [
    "stray argument",
    function() {
      return {
        buffer: emptyModuleBinary,
        args: [{}, {}],
        exports: {},
        verify: () => {}
      };
    }
  ],
];
