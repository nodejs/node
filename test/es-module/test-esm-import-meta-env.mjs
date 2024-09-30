// Flags: --experimental-import-meta-env
import '../common/index.mjs';
import assert from 'assert';
import { env } from 'process';

// Currently, the object is literally the same.
assert.strictEqual(import.meta.env, env);
assert.strictEqual(import.meta.env, process.env);

// It reflects changes to environment variables.
assert.strictEqual(import.meta.env.TEST_VAR_1, undefined);
process.env.TEST_VAR_1 = 'x';
assert.strictEqual(import.meta.env.TEST_VAR_1, 'x');

// It allows modifying environment variables.
assert.strictEqual(process.env.TEST_VAR_2, undefined);
import.meta.env.TEST_VAR_2 = 'x';
assert.strictEqual(process.env.TEST_VAR_2, 'x');

// It's possible to overwrite `import.meta.env`.
import.meta.env = 42;
assert.strictEqual(import.meta.env, 42);
