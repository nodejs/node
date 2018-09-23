'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var options = {stdio: ['pipe']};
var child = common.spawnPwd(options);

assert.notEqual(child.stdout, null);
assert.notEqual(child.stderr, null);

options = {stdio: 'ignore'};
child = common.spawnPwd(options);

assert.equal(child.stdout, null);
assert.equal(child.stderr, null);

options = {stdio: 'ignore'};
child = common.spawnSyncCat(options);
assert.deepEqual(options, {stdio: 'ignore'});
