import { allowGlobals, mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { strictEqual } from 'node:assert';
import { run } from 'node:test';

const stream = run({
  cwd: fixtures.path('test-runner', 'no-isolation'),
  isolation: 'none',
});

let errors = 0;
stream.on('test:fail', () => {
  errors++;
});
stream.on('test:pass', mustCall(5));
// eslint-disable-next-line no-unused-vars
for await (const _ of stream);
strictEqual(errors, 0);
allowGlobals(globalThis.GLOBAL_ORDER);
strictEqual(globalThis.GLOBAL_ORDER, [
  'before one: <root>',
  'suite one',
  'before two: <root>',
  'suite two',

  'beforeEach one: suite one - test',
  'beforeEach two: suite one - test',
  'suite one - test',
  'afterEach one: suite one - test',
  'afterEach two: suite one - test',

  'beforeEach one: test one',
  'beforeEach two: test one',
  'test one',
  'afterEach one: test one',
  'afterEach two: test one',

  'before suite two: suite two',

  'beforeEach one: suite two - test',
  'beforeEach two: suite two - test',
  'suite two - test',
  'afterEach one: suite two - test',
  'afterEach two: suite two - test',

  'after one: <root>',
  'after two: <root>',
]);
