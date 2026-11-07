'use strict';
const common = require('../common');
const assert = require('node:assert');
const { run, suite, test } = require('node:test');

// The skip option is documented as a truthy check, so falsy values other than
// false and undefined must run the test and not report it as skipped.
const falsyValues = ['', 0, null, NaN];

if (!process.env.NODE_TEST_CONTEXT) {
  const stream = run({ files: [__filename] });

  stream.on('test:fail', common.mustNotCall());
  stream.on('test:pass', common.mustCall((event) => {
    assert.strictEqual(event.skip, undefined);
  }, falsyValues.length * 2));
} else {
  for (const value of falsyValues) {
    test(`test skip: ${value}`, { skip: value }, common.mustCall());
    suite(`suite skip: ${value}`, { skip: value }, common.mustCall());
  }
}
