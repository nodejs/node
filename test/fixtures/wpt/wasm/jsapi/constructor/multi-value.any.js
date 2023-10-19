// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js
// META: script=/wasm/jsapi/assertions.js

const type_if_fi = makeSig([kWasmF64, kWasmI32], [kWasmI32, kWasmF64]);

promise_test(async () => {
  const builder = new WasmModuleBuilder();

  builder
    .addFunction("swap", type_if_fi)
    .addBody([
        kExprLocalGet, 1,
        kExprLocalGet, 0,
        kExprReturn,
    ])
    .exportFunc();

  const buffer = builder.toBuffer();

  const result = await WebAssembly.instantiate(buffer);
  const swapped = result.instance.exports.swap(4.2, 7);
  assert_true(Array.isArray(swapped));
  assert_equals(Object.getPrototypeOf(swapped), Array.prototype);
  assert_array_equals(swapped, [7, 4.2]);
}, "multiple return values from wasm to js");

promise_test(async () => {
  const builder = new WasmModuleBuilder();

  const swap = builder
    .addFunction("swap", type_if_fi)
    .addBody([
        kExprLocalGet, 1,
        kExprLocalGet, 0,
        kExprReturn,
    ]);
  builder
    .addFunction("callswap", kSig_i_v)
    .addBody([
        ...wasmF64Const(4.2),
        ...wasmI32Const(7),
        kExprCallFunction, swap.index,
        kExprDrop,
        kExprReturn,
    ])
    .exportFunc();

  const buffer = builder.toBuffer();

  const result = await WebAssembly.instantiate(buffer);
  const swapped = result.instance.exports.callswap();
  assert_equals(swapped, 7);
}, "multiple return values inside wasm");

promise_test(async () => {
  const builder = new WasmModuleBuilder();

  const fnIndex = builder.addImport("module", "fn", type_if_fi);
  builder
    .addFunction("callfn", kSig_i_v)
    .addBody([
        ...wasmF64Const(4.2),
        ...wasmI32Const(7),
        kExprCallFunction, fnIndex,
        kExprDrop,
        kExprReturn,
    ])
    .exportFunc();

  const buffer = builder.toBuffer();

  const actual = [];
  const imports = {
    "module": {
      fn(f32, i32) {
        assert_equals(f32, 4.2);
        assert_equals(i32, 7);
        const result = [2, 7.3];
        let i = 0;
        return {
          get [Symbol.iterator]() {
            actual.push("@@iterator getter");
            return function iterator() {
              actual.push("@@iterator call");
              return {
                get next() {
                  actual.push("next getter");
                  return function next(...args) {
                    assert_array_equals(args, []);
                    let j = ++i;
                    actual.push(`next call ${j}`);
                    if (j > result.length) {
                      return {
                        get done() {
                          actual.push(`done call ${j}`);
                          return true;
                        }
                      };
                    }
                    return {
                      get done() {
                        actual.push(`done call ${j}`);
                        return false;
                      },
                      get value() {
                        actual.push(`value call ${j}`);
                        return {
                          get valueOf() {
                            actual.push(`valueOf get ${j}`);
                            return function() {
                              actual.push(`valueOf call ${j}`);
                              return result[j - 1];
                            };
                          }
                        };
                      }
                    };
                  };
                }
              };
            }
          },
        };
      },
    }
  };

  const { instance } = await WebAssembly.instantiate(buffer, imports);
  const result = instance.exports.callfn();
  assert_equals(result, 2);
  assert_array_equals(actual, [
    "@@iterator getter",
    "@@iterator call",
    "next getter",
    "next call 1",
    "done call 1",
    "value call 1",
    "next call 2",
    "done call 2",
    "value call 2",
    "next call 3",
    "done call 3",
    "valueOf get 1",
    "valueOf call 1",
    "valueOf get 2",
    "valueOf call 2",
  ]);
}, "multiple return values from js to wasm");
