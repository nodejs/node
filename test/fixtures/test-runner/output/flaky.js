'use strict';
require('../../../common');
const { describe, it, test } = require('node:test');
const assert = require('node:assert');

// Deterministic, counter-based fixture for the `flaky` snapshot/output test.
// Each counter lives in module scope; retries happen in-process so the counter
// advances across attempts without any timing nondeterminism.

// Passes on the 3rd attempt (2 re-tries). Reports `# FLAKY 2 re-tries`.
let passLate = 0;
test('eventually passes', { flaky: 5 }, () => {
  passLate++;
  assert.strictEqual(passLate, 3);
});

// Marked flaky but passes on the first try: counted flaky, no re-tries directive.
test('flaky passes first try', { flaky: 3 }, () => {});

// A flaky suite: members inherit the suite's flaky budget.
let suiteMember = 0;
describe('flaky suite', { flaky: 4 }, () => {
  it('member retried then passes', () => {
    suiteMember++;
    assert.strictEqual(suiteMember, 2);
  });
  it('member passes immediately', () => {});
});

// Shorthand form retried then passes.
let shorthand = 0;
it.flaky('shorthand retried', () => {
  shorthand++;
  assert.strictEqual(shorthand, 2);
});

// Exhausts its retry budget and fails. Reports `# FLAKY failed after 2 re-tries`.
test('exhausts and fails', { flaky: 2 }, () => {
  throw new Error('always boom');
});
