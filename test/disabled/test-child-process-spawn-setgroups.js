'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

// Verify that setting groups actually works
// We become nobody/nogroup and ask to have disk as a secondary group
// uid and gid are based on Ubuntu 16.04 but are standard enough across
// Linux distributions and even other Unixes.
const dd = cp.spawn(
                      '/bin/dd', 
                      ['if=/dev/sda', 'of=/dev/null', 'bs=1k', 'count=1'], 
                      { 'uid': 65534, 'gid': 65534, 'groups': [ 6 ] }
                    );
let ddOutput = '';

dd.stdout.on('data', (data) => {
  ddOutput += data;
});
dd.on('close', common.mustCall((code, signal) => {
  assert.equal(code, 0);
}));

// We repeat the operation without setting the extra group, this should fail
const eaccessChild = cp.spawn('/bin/dd', 
        ['if=/dev/sda', 'of=/dev/null', 'bs=1k', 'count=1'], 
        { 'uid': 65534, 'gid': 65534 }
      );
eaccessChild.on('error', function(err) {
  assert.equal(err.code, 'EACCES');
  assert.equal(err.errno, 'EACCES');
  assert.equal(err.syscall, 'spawn ' + enoentPath);
  assert.equal(err.path, enoentPath);
  assert.deepStrictEqual(err.spawnargs, spawnargs);
});