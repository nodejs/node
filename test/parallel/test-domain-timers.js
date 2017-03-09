'use strict';
const common = require('../common');
const domain = require('domain');
const assert = require('assert');

let timeout;

const timeoutd = domain.create();

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

const immediated = domain.create();

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
