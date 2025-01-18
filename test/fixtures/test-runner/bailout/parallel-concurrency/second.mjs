import { describe, it } from 'node:test';

describe('second parallel file', () => {
  it('slow test that should be cancelled', async () => {
    await new Promise((resolve) => setTimeout(resolve, 5000));
  });
});
