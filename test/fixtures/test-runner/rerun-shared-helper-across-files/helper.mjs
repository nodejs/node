import { describe, it } from 'node:test';
import assert from 'node:assert';

// Every test file calling this registers its suites and tests at the same
// source location, which is outside of the test file itself.
export function describeEngine(name, { fails = false } = {}) {
  describe(name, () => {
    it('works', ({ attempt }) => {
      assert.ok(!fails, `${name} always fails`);
      assert.strictEqual(attempt, 0, `${name} already passed and must not run again`);
    });
  });
}
