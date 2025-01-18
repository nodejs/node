import { describe, it, before } from 'node:test';

describe('test-with-setup', { concurrency: false }, () => {
  before(async () => {
    // Give some time to the concurrent test to start
    await new Promise((resolve) => setTimeout(resolve, 2500));
  });

  it('first', async () => {
    throw new Error('First test failed');
  });

  it('second', () => {
    throw new Error('Second test should not run');
  });
});
