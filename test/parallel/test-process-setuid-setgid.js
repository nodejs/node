'use strict';

const common = require('../common');

const assert = require('assert');

if (common.isWindows) {
  // uid/gid functions are POSIX only
  assert.strictEqual(process.getuid, undefined);
  assert.strictEqual(process.setuid, undefined);
  assert.strictEqual(process.getgid, undefined);
  assert.strictEqual(process.setgid, undefined);
  return;
}

assert.throws(() => {
  process.setuid('fhqwhgadshgnsdhjsdbkhsdabkfabkveybvf');
}, /^Error: setuid user id does not exist$/);

// If we're not running as super user...
if (process.getuid() !== 0) {
  assert.doesNotThrow(() => {
    process.getgid();
    process.getuid();
  });

  assert.throws(
    () => { process.setgid('nobody'); },
    /^Error: (?:EPERM, .+|setgid group id does not exist)$/
  );

  assert.throws(
    () => { process.setuid('nobody'); },
    /^Error: (?:EPERM, .+|setuid user id does not exist)$/
  );
  return;
}

// If we are running as super user...
const oldgid = process.getgid();
process.setgid('nobody');
const newgid = process.getgid();
assert.notStrictEqual(newgid, oldgid);

const olduid = process.getuid();
process.setuid('nobody');
const newuid = process.getuid();
assert.notStrictEqual(newuid, olduid);
