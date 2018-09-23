'use strict';
var domain = require('domain');
var assert = require('assert');
var common = require('../common');

var timeout_err, timeout, immediate_err;

var timeoutd = domain.create();

timeoutd.on('error', function(e) {
  timeout_err = e;
  clearTimeout(timeout);
});

timeoutd.run(function() {
  setTimeout(function() {
    throw new Error('Timeout UNREFd');
  }).unref();
});

var immediated = domain.create();

immediated.on('error', function(e) {
  immediate_err = e;
});

immediated.run(function() {
  setImmediate(function() {
    throw new Error('Immediate Error');
  });
});

timeout = setTimeout(function() {}, 10 * 1000);

process.on('exit', function() {
  assert.equal(timeout_err.message, 'Timeout UNREFd',
      'Domain should catch timer error');
  assert.equal(immediate_err.message, 'Immediate Error',
      'Domain should catch immediate error');
});
