import { mock, test } from 'node:test';

const dependency = mock.fn(() => 'mock-return-value');
mock.module('../coverage-with-mock/dependency.cjs', { namedExports: { dependency } });

const { subject } = await import('../coverage-with-mock/subject.mjs');

test('subject calls dependency', (t) => {
  t.assert.strictEqual(subject(), 'mock-return-value');
  t.assert.strictEqual(dependency.mock.callCount(), 1);
});
