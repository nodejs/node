'use strict';

const common = require('../common');
const assert = require('assert');
const domain = require('domain').create();
const EventEmitter = require('events');

domain.on('error', common.mustNotCall());

const ee = new EventEmitter();

const plainObject = { justAn: 'object' };
ee.once('error', common.mustCall((err) => {
  assert.deepStrictEqual(err, plainObject);
}));
ee.emit('error', plainObject);

const err = new Error('test error');
ee.once('error', common.expectsError(err));
ee.emit('error', err);
