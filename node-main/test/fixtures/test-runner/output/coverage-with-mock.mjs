import { describe, it, mock } from 'node:test';

describe('module test with mock', async () => {
  mock.module('../coverage-with-mock/sum.js', {
    namedExports: {
      sum: (a, b) => 1,
      getData: () => ({}),
    },
  });

  const { theModuleSum, theModuleGetData } = await import('../coverage-with-mock/module.js');

  it('tests correct thing', (t) => {
    t.assert.strictEqual(theModuleSum(1, 2), 1);
    t.assert.deepStrictEqual(theModuleGetData(), {});
  });
});
