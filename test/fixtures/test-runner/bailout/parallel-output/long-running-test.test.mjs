import { describe, it } from 'node:test';

describe('long-running-test', async () => {
  it('first', async (t) => {
    t.assert.ok(true, 'First test passed');
  });

  it('second', async () => {
    while (true) {
      // a long running test
      await new Promise((resolve) => setTimeout(resolve, 2500));
    }
    throw new Error('Second test should not run');
  });
});
