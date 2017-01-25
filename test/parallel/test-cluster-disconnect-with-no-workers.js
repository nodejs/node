'use strict';
require('../common');
const assert = require('assert');
const cluster = require('cluster');

let disconnected;

process.on('exit', function() {
  assert(disconnected);
});

cluster.disconnect(function() {
  disconnected = true;
});

// Assert that callback is not sometimes synchronous
assert(!disconnected);
