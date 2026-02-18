import { describe, it, mock } from 'node:test';

describe('coverage with mocked CJS module in ESM', async () => {
  mock.module('../coverage-with-mock-cjs/dependency.cjs', {
    namedExports: {
      sum: (a, b) => 42,
      getData: () => ({ mocked: true }),
    },
  });

  const { theModuleSum, theModuleGetData } = await import('../coverage-with-mock-cjs/subject.mjs');

  it('uses mocked CJS exports', (t) => {
    t.assert.strictEqual(theModuleSum(1, 2), 42);
    t.assert.deepStrictEqual(theModuleGetData(), { mocked: true });
  });
});
