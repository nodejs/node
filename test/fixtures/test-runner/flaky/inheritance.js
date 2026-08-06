'use strict';
const { describe, it } = require('node:test');
const assert = require('node:assert');
let inheritAttempts = 0;
let overrideAttempts = 0;
let disabledAttempts = 0;
describe('flaky suite', { flaky: 5 }, () => {
  it('inherits suite flaky', () => {
    inheritAttempts++;
    assert.strictEqual(inheritAttempts, 3); // needs 2 retries
  });
  it('overrides with own flaky', { flaky: 1 }, () => {
    overrideAttempts++;
    assert.strictEqual(overrideAttempts, 2); // passes on attempt 2 (1 retry)
  });
  it('opted out via flaky:false', { flaky: false }, () => {
    disabledAttempts++;
    assert.strictEqual(disabledAttempts, 99); // never reaches; must NOT retry
  });
});
