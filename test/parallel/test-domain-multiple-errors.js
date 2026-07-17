'use strict';

const common = require('../common');
const assert = require('assert');
const domain = require('domain');

// This test is similar to test-domain-error-types, but uses a single domain
// to emit all errors.

const d = new domain.Domain();

const values = [
  42, null, undefined, false, () => {}, 'string', Symbol('foo'),
];

d.on('error', common.mustCall((err) => {
  assert(values.includes(err));
}, values.length));

for (const something of values) {
  d.run(common.mustCall(() => {
    process.nextTick(common.mustCall(() => {
      throw something;
    }));
  }));
}
