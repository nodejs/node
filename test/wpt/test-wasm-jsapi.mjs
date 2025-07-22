// Flags: --experimental-wasm-modules
import * as fixtures from '../common/fixtures.mjs';
import { ok } from 'node:assert';
import { WPTRunner } from '../common/wpt.js';

// Verify we have Wasm SIMD support by importing a Wasm with SIMD
// since Wasm SIMD is not supported on older architectures such as IBM Power8.
let supportsSimd = false;
try {
  await import(fixtures.fileURL('es-modules/globals.wasm'));
  supportsSimd = true;
} catch (e) {
  ok(e instanceof WebAssembly.CompileError);
  ok(e.message.includes('SIMD unsupported'));
}

if (supportsSimd) {
  const runner = new WPTRunner('wasm/jsapi');
  runner.setFlags(['--experimental-wasm-modules']);
  runner.runJsTests();
}
