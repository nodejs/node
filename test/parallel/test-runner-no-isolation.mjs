import { allowGlobals, mustCall, mustNotCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { run } from 'node:test';

const stream = run({
  files: [
    fixtures.path('test-runner', 'no-isolation', 'one.test.js'),
    fixtures.path('test-runner', 'no-isolation', 'two.test.js'),
  ],
  isolation: 'none',
});

stream.on('test:fail', mustNotCall());
stream.on('test:pass', mustCall(6));
// eslint-disable-next-line no-unused-vars
for await (const _ of stream);
allowGlobals(globalThis.GLOBAL_ORDER);

// Each file is wrapped in an implicit suite when using isolation: 'none',
// so top-level hooks (before, beforeEach, etc.) are scoped to the file
// and do not leak across test files.
const file1 = fixtures.path('test-runner', 'no-isolation', 'one.test.js');
const file2 = fixtures.path('test-runner', 'no-isolation', 'two.test.js');
assert.deepStrictEqual(globalThis.GLOBAL_ORDER, [
  'suite one',
  'suite two',

  `before one: ${file1}`,
  'beforeEach one: suite one - test',
  'suite one - test',
  'afterEach one: suite one - test',
  `after one: ${file1}`,

  `before two: ${file2}`,
  'before suite two: suite two',
  'beforeEach two: suite two - test',
  'suite two - test',
  'afterEach two: suite two - test',
  `after two: ${file2}`,
]);
