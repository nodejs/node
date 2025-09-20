import '../common/index.mjs';
import { strictEqual } from 'node:assert';
import { readSync } from '../common/fixtures.mjs';

// Test Wasm JSPI
{
  const asyncImport = async (x) => {
    await new Promise((resolve) => setTimeout(resolve, 10));
    return x + 42;
  };

  const { instance } = await WebAssembly.instantiate(readSync('wasm/jspi.wasm'), {
    js: {
      async: new WebAssembly.Suspending(asyncImport),
    },
  });

  const promisingExport = WebAssembly.promising(instance.exports.test);
  strictEqual(await promisingExport(10), 52);
}
