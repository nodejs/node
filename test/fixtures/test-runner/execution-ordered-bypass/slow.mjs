import { test } from 'node:test';
import { setTimeout as sleep } from 'node:timers/promises';

test('slow', async () => {
  // Long enough that fast-fail's process can spawn, run, and round-trip its
  // bypassed test:complete to the host on slow CI, but short enough that the
  // test does not waste much time when the bypass is working.
  await sleep(30_000);
});
