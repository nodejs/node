import { suite, test } from 'node:test';
import { setTimeout } from 'node:timers/promises';

suite('my suite', (s) => {
  s.log('suite message');
  test('in suite', () => {});
});

test('parent', { concurrency: 2 }, async (t) => {
  await Promise.all([
    t.test('slow', async () => {
      await setTimeout(200);
    }),
    t.test('logger', (t) => {
      t.log('hello', { foo: 1 });
      t.log('warned', { level: 'warn', attempt: 2 });
    }),
  ]);
});
