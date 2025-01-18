import { describe, it } from 'node:test';

describe('infinite loop test', () => {
  it('infinite loop that should be cancelled', async () => {
    // This should be cancelled by the bail
    while (true) {
      await new Promise(resolve => setTimeout(resolve, 100));
    }
  });
});
