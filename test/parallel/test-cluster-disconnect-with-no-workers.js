'use strict';
require('../common');
var assert = require('assert');
var cluster = require('cluster');

var disconnected;

process.on('exit', function() {
  assert(disconnected);
});

cluster.disconnect(function() {
  disconnected = true;
});

// Assert that callback is not sometimes synchronous
assert(!disconnected);
