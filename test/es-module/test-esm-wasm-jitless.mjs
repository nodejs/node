// Flags: --jitless
import '../common/index.mjs';
import assert from 'node:assert/strict';
import * as fixtures from '../common/fixtures.mjs';

const url = fixtures.fileURL('es-modules/simple.wasm').href;
const error = {
  code: 'ERR_WEBASSEMBLY_NOT_SUPPORTED',
  message: 'WebAssembly is not supported in this environment, but is required for WebAssembly module imports',
};

await assert.rejects(import(url), error);
await assert.rejects(import.source(url), error);
for (const statement of [
  `import * as mod from ${JSON.stringify(url)};`,
  `import source mod from ${JSON.stringify(url)};`,
]) {
  await assert.rejects(import(`data:text/javascript,${encodeURIComponent(statement)}`), error);
}

// Catching the missing capability must leave ordinary module imports usable.
assert.strictEqual((await import('data:text/javascript,export default 42')).default, 42);
