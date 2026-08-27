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

assert.strictEqual(isBuiltin('node:bench'), true);
assert.strictEqual(isBuiltin('bench'), false);
assert.strictEqual(builtinModules.includes('node:bench'), true);
assert.strictEqual(process.getBuiltinModule('node:bench'), benchDefault);
assert.strictEqual(process.getBuiltinModule('bench'), undefined);

const require = createRequire(import.meta.url);
assert.throws(() => require('bench'), { code: 'MODULE_NOT_FOUND' });
await assert.rejects(import('bench'), { code: 'ERR_MODULE_NOT_FOUND' });
