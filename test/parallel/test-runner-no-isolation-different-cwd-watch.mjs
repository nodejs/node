import { allowGlobals, mustCall, mustNotCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { deepStrictEqual } from 'node:assert';
import { run } from 'node:test';

try {
  const controller = new AbortController();
  const stream = run({
    cwd: fixtures.path('test-runner', 'no-isolation'),
    isolation: 'none',
    watch: true,
    signal: controller.signal,
  });

  stream.on('test:fail', () => mustNotCall());
  stream.on('test:pass', mustCall(5));
  stream.on('data', function({ type }) {
    if (type === 'test:watch:drained') {
      controller.abort();
    }
  });
  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
  allowGlobals(globalThis.GLOBAL_ORDER);
  deepStrictEqual(globalThis.GLOBAL_ORDER, [
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
} catch (err) {
  console.error(err);
  process.exit(1);
}
