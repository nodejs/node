'use strict';
const common = require('../common');
const assert = require('assert');
const events = require('events');
const domain = require('domain');
const e = new events.EventEmitter();

const d = domain.create();
d.add(e);
d.on('error', common.mustCall(function(er) {
  assert(er instanceof Error, 'error created');
}));

e.emit('error');
