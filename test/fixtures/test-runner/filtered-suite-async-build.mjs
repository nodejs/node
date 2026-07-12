import test from 'node:test';
import { setTimeout as delay } from 'node:timers/promises';

test.suite('Outer', async () => {
  await delay(1);

  // This suite is filtered out by name. Its build is still pending when the
  // filtered run starts, so the subtest below is registered late.
  test.suite('Nested A', async () => {
    await delay(1);
    test('Nested A test', async () => {});
  });

  test.suite('Nested C', async () => {
    test('Nested C test', async () => {});
  });
});
