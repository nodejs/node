import test from 'node:test';
import { setTimeout as sleep } from 'node:timers/promises';

test('parent', { timeout: 50, concurrency: 1 }, async (t) => {
  t.test('first', async () => {
    await sleep(1000);
  });

  t.test('second', async () => {});
  t.test('third', async () => {});
});
