'use strict';

const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const EventEmitter = require('events');

const d = new domain.Domain();
let implicit;

d.on('error', common.mustCall((err) => {
  assert.strictEqual(err.message, 'foobar');
  assert.strictEqual(err.domain, d);
  assert.strictEqual(err.domainEmitter, implicit);
  assert.strictEqual(err.domainBound, undefined);
  assert.strictEqual(err.domainThrown, false);
}));

// Implicit addition of the EventEmitter by being created within a domain-bound
// context.
d.run(common.mustCall(() => {
  implicit = new EventEmitter();
}));

setTimeout(common.mustCall(() => {
  // escape from the domain, but implicit is still bound to it.
  implicit.emit('error', new Error('foobar'));
}), 1);
