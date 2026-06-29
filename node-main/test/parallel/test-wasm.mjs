import '../common/index.mjs';
import { strictEqual } from 'node:assert';
import { readSync } from '../common/fixtures.mjs';

// Test Wasm JSPI
{
  const asyncImport = async (x) => {
    await new Promise((resolve) => setTimeout(resolve, 10));
    return x + 42;
  };

  /**
   * wasm/jspi.wasm contains:
   *
   * (module
   *   (type (;0;) (func (param i32) (result i32)))
   *   (import "js" "async" (func $async (;0;) (type 0)))
   *   (export "test" (func $test))
   *   (func $test (;1;) (type 0) (param $x i32) (result i32)
   *     local.get $x
   *     call $async
   *   )
   * )
   *
   * Which is the JS equivalent to:
   *
   * import { async_ } from 'js';
   * export function test (val) {
   *   return async_(val);
   * }
   *
   * JSPI then allows turning this sync style Wasm call into async code
   * that suspends on the promise.
   */

  const { instance } = await WebAssembly.instantiate(readSync('wasm/jspi.wasm'), {
    js: {
      async: new WebAssembly.Suspending(asyncImport),
    },
  });

  const promisingExport = WebAssembly.promising(instance.exports.test);
  strictEqual(await promisingExport(10), 52);
}
