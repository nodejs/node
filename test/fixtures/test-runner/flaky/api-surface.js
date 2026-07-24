'use strict';
const test = require('node:test');
const { describe, it, suite } = test;
const assert = require('node:assert');

// Form 1: describe.flaky shorthand suite; members inherit.
describe.flaky('describe.flaky suite', () => {
  let n = 0;
  it('member inherits describe.flaky', () => { n++; assert.strictEqual(n, 2); });
});

// Form 2: it.flaky shorthand test-case.
it.flaky('it.flaky case', (() => { let n = 0; return () => { n++; assert.strictEqual(n, 2); }; })());

// Form 3: describe options form { flaky: true }; members inherit.
describe('describe options suite', { flaky: true }, () => {
  let n = 0;
  it('member inherits options flaky', () => { n++; assert.strictEqual(n, 2); });
});

// Form 4: it options form, boolean.
it('it options boolean', { flaky: true }, (() => { let n = 0; return () => { n++; assert.strictEqual(n, 2); }; })());

// Form 5: it options form, positive integer.
it('it options integer', { flaky: 100 }, (() => { let n = 0; return () => { n++; assert.strictEqual(n, 2); }; })());

// Alias: test.flaky shorthand.
test.flaky('test.flaky case', (() => { let n = 0; return () => { n++; assert.strictEqual(n, 2); }; })());

// Alias: suite.flaky shorthand.
suite.flaky('suite.flaky suite', () => {
  let n = 0;
  it('member inherits suite.flaky', () => { n++; assert.strictEqual(n, 2); });
});

// Alias: test('name', { flaky: N }, fn).
test('test options integer', { flaky: 7 }, (() => { let n = 0; return () => { n++; assert.strictEqual(n, 2); }; })());
