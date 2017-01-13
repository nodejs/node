'use strict';
// Requires special privileges
const common = require('../common');
const assert = require('assert');

var oldgid = process.getgid();
process.setgid('nobody');
var newgid = process.getgid();
assert.notStrictEqual(newgid, oldgid, 'gids expected to be different');

var olduid = process.getuid();
process.setuid('nobody');
var newuid = process.getuid();
assert.notStrictEqual(newuid, olduid, 'uids expected to be different');

try {
  process.setuid('nobody1234');
} catch (e) {
  assert.strictEqual(e.message,
               'failed to resolve group',
               'unexpected error message'
  );
}
