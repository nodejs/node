'use strict';

const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const EventEmitter = require('events');

const d = new domain.Domain();
const e = new EventEmitter();

d.on('error', common.mustCall((err) => {
  assert.strictEqual(err.message, 'foobar');
  assert.strictEqual(err.domain, d);
  assert.strictEqual(err.domainEmitter, e);
  assert.strictEqual(err.domainBound, undefined);
  assert.strictEqual(err.domainThrown, false);
}));

d.add(e);
e.emit('error', new Error('foobar'));
