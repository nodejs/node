// Regression coverage for https://github.com/nodejs/node/issues/63424:
// `--test-rerun-failures` could mark a failing test as passing on retry
// without executing its body.
//
// The runtime disambiguator at lib/internal/test_runner/test.js keys tests
// by `file:line:column`. A `t.test()` registered inside a factory function
// gets the same source location regardless of which parent invoked the
// factory. Three independent bugs interacted to make the failure-on-retry
// vanish:
//   1. Runner counter was set against the suffixed key, so it never
//      advanced past 1 - every 3rd+ same-loc registration collided on :(1).
//   2. Reporter had the same off-by-one against the suffixed key.
//   3. Reporter only bumped its counter on test:pass, so failing tests at
//      a shared location desynchronised the writer and runner counters.
// On retry, the failing sibling could inherit a counter slot that, in
// attempt 0, belonged to a different (passing) sibling. Node matched by
// that slot, replaced `this.fn` with a synthetic noop replay, and reported
// the failure as a pass.

import { describe, it } from 'node:test';
import { strict as assert } from 'node:assert';

function makeSuite(shouldPass, label) {
  return async (t) => {
    await t.test('inner', async () => {
      if (!shouldPass) assert.fail(`${label} should fail`);
    });
  };
}

// Four siblings with the failure in the middle, placed first so that the
// passing sibling at global position 2 (E) ends up recorded at base:(1).
// With the runner-side off-by-one (bug 1), the buggy counter on retry would
// alias F's id onto base:(1), match F against E's recorded "passed" entry,
// replace F's assert.fail with a synthetic noop, and swallow F.
describe('parents (middle failure)', { concurrency: false }, () => {
  it('D passes', makeSuite(true,  'D'));
  it('E passes', makeSuite(true,  'E'));
  it('F fails',  makeSuite(false, 'F'));   // the only real failure in this group
  it('G passes', makeSuite(true,  'G'));
});

// Three-sibling case from the issue, kept verbatim to exercise the writer
// bugs (off-by-one storing the counter against the suffixed key; reporter
// ignoring test:fail when bumping the counter). Either bug shifts C's
// recorded slot from base:(6) to a position B would inherit on retry.
describe('parents', { concurrency: false }, () => {
  it('A passes', makeSuite(true,  'A'));
  it('B fails',  makeSuite(false, 'B'));   // the only real failure in this group
  it('C passes', makeSuite(true,  'C'));
});
