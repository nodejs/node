'use strict';
const common = require('../common');
var domain = require('domain');
var assert = require('assert');

var timeout;

var timeoutd = domain.create();

timeoutd.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.message, 'Timeout UNREFd',
                     'Domain should catch timer error');
  clearTimeout(timeout);
}));

timeoutd.run(function() {
  setTimeout(function() {
    throw new Error('Timeout UNREFd');
  }, 0).unref();
});

var immediated = domain.create();

immediated.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.message, 'Immediate Error',
                     'Domain should catch immediate error');
}));

immediated.run(function() {
  setImmediate(function() {
    throw new Error('Immediate Error');
  });
});

timeout = setTimeout(function() {}, 10 * 1000);
