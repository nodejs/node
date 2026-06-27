import { allowGlobals, mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { run } from 'node:test';

const stream = run({
  cwd: fixtures.path('test-runner', 'no-isolation'),
  isolation: 'none',
});


stream.on('test:pass', mustCall(6));
// eslint-disable-next-line no-unused-vars
for await (const _ of stream);
allowGlobals(globalThis.GLOBAL_ORDER);
assert.deepStrictEqual(globalThis.GLOBAL_ORDER, [
  'suite one',
  'suite two',
  'before one: one.test.js',
  'beforeEach one: suite one - test',
  'suite one - test',
  'afterEach one: suite one - test',
  'after one: one.test.js',
  'before two: two.test.js',
  'before suite two: suite two',
  'beforeEach two: suite two - test',
  'suite two - test',
  'afterEach two: suite two - test',
  'after two: two.test.js',
]);
