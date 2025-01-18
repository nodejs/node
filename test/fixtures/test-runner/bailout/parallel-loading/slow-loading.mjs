import { describe, it, before } from 'node:test';

describe('slow loading test', () => {
  before(async () => {
    // Simulate slow loading
    await new Promise((resolve) => setTimeout(resolve, 1000));
  });

  it('failing test after slow load', () => {
    throw new Error('Test failed after slow load');
  });
});
