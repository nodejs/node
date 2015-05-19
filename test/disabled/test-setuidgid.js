'use strict';
// Requires special privileges
var common = require('../common');
var assert = require('assert');

var oldgid = process.getgid();
process.setgid('nobody');
var newgid = process.getgid();
assert.notEqual(newgid, oldgid, 'gids expected to be different');

var olduid = process.getuid();
process.setuid('nobody');
var newuid = process.getuid();
assert.notEqual(newuid, olduid, 'uids expected to be different');

try {
  process.setuid('nobody1234');
} catch (e) {
  assert.equal(e.message,
               'failed to resolve group',
               'unexpected error message'
  );
}
