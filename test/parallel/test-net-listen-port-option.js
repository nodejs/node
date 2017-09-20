'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

function close() { this.close(); }
net.Server().listen({ port: undefined }, close);
net.Server().listen({ port: '' + common.PORT }, close);

[ 'nan',
  -1,
  123.456,
  0x10000,
  1 / 0,
  -1 / 0,
  '+Infinity',
  '-Infinity'
].forEach(function(port) {
  assert.throws(function() {
    net.Server().listen({ port: port }, common.fail);
  }, /"port" argument must be >= 0 and < 65536/i);
});

[null, true, false].forEach(function(port) {
  assert.throws(function() {
    net.Server().listen({ port: port }, common.fail);
  }, /invalid listen argument/i);
});

// Repeat the tests, passing port as an argument, which validates somewhat
// differently.

net.Server().listen(undefined, close);
net.Server().listen('0', close);

// 'nan', skip, treated as a path, not a port
//'+Infinity', skip, treated as a path, not a port
//'-Infinity' skip, treated as a path, not a port

// 4.x treats these as 0, but 6.x treats them as invalid numbers.
[
  -1,
  123.456,
  0x10000,
  1 / 0,
  -1 / 0,
].forEach(function(port) {
  assert.throws(function() {
    net.Server().listen(port, common.fail);
  }, /"port" argument must be >= 0 and < 65536/i);
});

// null is treated as 0
net.Server().listen(null, close);

// false/true are converted to 0/1, arguably a bug, but fixing would be
// semver-major. Note that true fails when port 1 low can't be listened on by
// unprivileged processes (Linux) but the listen does succeed on some Windows
// versions.
net.Server().listen(false, close);

(function() {
  const done = common.mustCall(function(err) {
    if (err)
      return assert.strictEqual(err.code, 'EACCES');

    assert.strictEqual(this.address().port, 1);
    this.close();
  });

  net.Server().listen(true).on('error', done).on('listening', done);
})();
