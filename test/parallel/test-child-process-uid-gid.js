'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const fs = require('fs');

const uid = 0;
const gid = 0;

assert.throws(() => {
  spawn('echo', ['fhqwhgads'], {uid: 0});
}, /EPERM/, 'Setting UID should throw EPERM for unprivileged users.');

assert.throws(() => {
  spawn('echo', ['fhqwhgads'], {gid: 0});
}, /EPERM/, 'Setting GID should throw EPERM for unprivileged users.');
