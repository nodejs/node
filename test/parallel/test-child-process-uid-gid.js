'use strict';
require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

assert.throws(() => {
  spawn('echo', ['fhqwhgads'], {uid: 0});
}, /EPERM/, 'Setting UID should throw EPERM for unprivileged users.');

assert.throws(() => {
  spawn('echo', ['fhqwhgads'], {gid: 0});
}, /EPERM/, 'Setting GID should throw EPERM for unprivileged users.');
