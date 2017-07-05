'use strict';

const common = require('../common');
const assert = require('assert');

if (common.isWindows) {
  assert.strictEqual(process.geteuid, undefined);
  assert.strictEqual(process.getegid, undefined);
  assert.strictEqual(process.seteuid, undefined);
  assert.strictEqual(process.setegid, undefined);
  return;
}

assert.throws(() => {
  process.seteuid({});
}, /^TypeError: seteuid argument must be a number or string$/);

assert.throws(() => {
  process.seteuid('fhqwhgadshgnsdhjsdbkhsdabkfabkveybvf');
}, /^Error: seteuid user id does not exist$/);

// If we're not running as super user...
if (process.getuid() !== 0) {
  assert.doesNotThrow(() => {
    process.getegid();
    process.geteuid();
  });

  assert.throws(() => {
    process.setegid('nobody');
  }, /^Error: (?:EPERM, .+|setegid group id does not exist)$/);

  assert.throws(() => {
    process.seteuid('nobody');
  }, /^Error: (?:EPERM, .+|seteuid user id does not exist)$/);

  return;
}

// If we are running as super user...
const oldgid = process.getegid();
process.setegid('nobody');
const newgid = process.getegid();
assert.notStrictEqual(newgid, oldgid);

const olduid = process.geteuid();
process.seteuid('nobody');
const newuid = process.geteuid();
assert.notStrictEqual(newuid, olduid);
