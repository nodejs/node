import assert from 'node:assert';
import { entrypoint } from 'node:process';

assert.strictEqual(entrypoint, process.env.NODE_TEST_ENTRYPOINT);
