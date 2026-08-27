// Flags: --no-warnings

import '../common/index.mjs';
import assert from 'node:assert';
import { createRequire, builtinModules, isBuiltin } from 'node:module';
import benchDefault, {
  after,
  afterEach,
  before,
  beforeEach,
  bench,
  describe,
  run,
  suite,
} from 'node:bench';
import { json, spec } from 'node:bench/reporters';

assert.strictEqual(benchDefault, bench);
assert.strictEqual(describe, suite);
for (const value of [
  after,
  afterEach,
  before,
  beforeEach,
  bench,
  run,
  suite,
]) {
  assert.strictEqual(typeof value, 'function');
}
assert.strictEqual(typeof bench.skip, 'function');
assert.strictEqual(typeof bench.only, 'function');
assert.strictEqual(typeof json, 'function');
assert.strictEqual(typeof spec, 'function');

assert.strictEqual(isBuiltin('node:bench'), true);
assert.strictEqual(isBuiltin('node:bench/reporters'), true);
assert.strictEqual(isBuiltin('bench'), false);
assert.strictEqual(isBuiltin('bench/reporters'), false);
assert.strictEqual(builtinModules.includes('node:bench'), true);
assert.strictEqual(builtinModules.includes('node:bench/reporters'), true);
assert.strictEqual(process.getBuiltinModule('node:bench'), benchDefault);
assert.strictEqual(process.getBuiltinModule('bench'), undefined);

const require = createRequire(import.meta.url);
const reporters = require('node:bench/reporters');
assert.strictEqual(reporters.json, json);
assert.strictEqual(reporters.spec, spec);
assert.throws(() => require('bench'), { code: 'MODULE_NOT_FOUND' });
assert.throws(() => require('bench/reporters'), { code: 'MODULE_NOT_FOUND' });
await assert.rejects(import('bench'), { code: 'ERR_MODULE_NOT_FOUND' });
await assert.rejects(import('bench/reporters'), {
  code: 'ERR_MODULE_NOT_FOUND',
});
