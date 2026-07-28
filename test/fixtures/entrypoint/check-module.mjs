import assert from 'node:assert';
import { entrypoint } from 'node:process';

assert.strictEqual(entrypoint.href, process.env.NODE_TEST_ENTRYPOINT);
