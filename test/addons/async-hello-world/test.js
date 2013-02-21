var assert = require('assert');
var binding = require('./build/Release/binding');
var called = false;

process.on('exit', function () {
  assert(called);
});

binding(5, function (err, val) {
  assert.equal(null, err);
  assert.equal(10, val);
  process.nextTick(function () {
    called = true;
  });
});
