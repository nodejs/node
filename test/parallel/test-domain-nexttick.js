'use strict';

const common = require('../common');
const assert = require('assert');
const domain = require('domain');

const d = new domain.Domain();

d.on('error', common.mustCall((err) => {
  assert.strictEqual(err.message, 'foobar');
  assert.strictEqual(err.domain, d);
  assert.strictEqual(err.domainEmitter, undefined);
  assert.strictEqual(err.domainBound, undefined);
  assert.strictEqual(err.domainThrown, true);
}));

d.run(common.mustCall(() => {
  process.nextTick(common.mustCall(() => {
    throw new Error('foobar');
  }));
}));
