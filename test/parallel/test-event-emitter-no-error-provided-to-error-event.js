'use strict';
const common = require('../common');
var assert = require('assert');
var events = require('events');
var domain = require('domain');
var e = new events.EventEmitter();

var d = domain.create();
d.add(e);
d.on('error', common.mustCall(function(er) {
  assert(er instanceof Error, 'error created');
}));

e.emit('error');
