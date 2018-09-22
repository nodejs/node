'use strict';
const common = require('../common');
if (process.config.variables.node_without_node_options)
  common.skip('missing NODE_OPTIONS support');
if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');

// Test options specified by env variable.

const assert = require('assert');
const exec = require('child_process').execFile;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
process.chdir(tmpdir.path);

disallow('--version');
disallow('-v');
disallow('--help');
disallow('-h');
disallow('--eval');
disallow('-e');
disallow('--print');
disallow('-p');
disallow('-pe');
disallow('--check');
disallow('-c');
disallow('--interactive');
disallow('-i');
disallow('--v8-options');
disallow('--');

function disallow(opt) {
  const env = Object.assign({}, process.env, { NODE_OPTIONS: opt });
  exec(process.execPath, { env }, common.mustCall(function(err) {
    const message = err.message.split(/\r?\n/)[1];
    const expect = `${process.execPath}: ${opt} is not allowed in NODE_OPTIONS`;

    assert.strictEqual(err.code, 9);
    assert.strictEqual(message, expect);
  }));
}
