'use strict';
var common = require('../common');
var assert = require('assert');
var events = require('events');
var domain = require('domain');

var errorCatched = false;

var e = new events.EventEmitter();

var d = domain.create();
d.add(e);
d.on('error', function(er) {
  assert(er instanceof Error, 'error created');
  errorCatched = true;
});

e.emit('error');

process.on('exit', function() {
  assert(errorCatched, 'error got caught');
});
