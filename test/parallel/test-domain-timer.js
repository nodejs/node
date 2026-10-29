'use strict';

const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const isEnumerable = Function.call.bind(Object.prototype.propertyIsEnumerable);

const d = new domain.Domain();

d.on('error', common.mustCall((err) => {
  assert.strictEqual(err.message, 'foobar');
  assert.strictEqual(err.domain, d);
  assert.strictEqual(isEnumerable(err, 'domain'), false);
  assert.strictEqual(err.domainEmitter, undefined);
  assert.strictEqual(err.domainBound, undefined);
  assert.strictEqual(err.domainThrown, true);
}));

d.run(common.mustCall(() => {
  setTimeout(common.mustCall(() => {
    throw new Error('foobar');
  }), 1);
}));
