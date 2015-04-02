var assert = require('assert');
var common = require('../common');
var i = 0;

process.on('beforeExit', function() {
  assert.ok(++i <= 1);
  setTimeout(function() { }, 100).unref();
});
