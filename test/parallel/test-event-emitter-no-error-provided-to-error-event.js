'use strict';
const common = require('../common');
const assert = require('assert');
const events = require('events');
const domain = require('domain');
var e = new events.EventEmitter();

var d = domain.create();
d.add(e);
d.on('error', common.mustCall(function(er) {
  assert(er instanceof Error, 'error created');
}));

e.emit('error');
