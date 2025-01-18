import { describe, it } from 'node:test';

describe('first parallel file', () => {

  it('passing test with delay', async (t) => {
    await new Promise((resolve) => setTimeout(resolve, 200));
    t.assert.ok(true);
  });

  it('failing test', () => {
    throw new Error('First test failed');
  });
});
