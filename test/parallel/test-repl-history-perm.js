'use strict';
// Flags: --expose_internals

const common = require('../common');

if (common.isWindows) {
  common.skip('Win32 uses ACLs for file permissions, ' +
              'modes are always 0666 and says nothing about group/other ' +
              'read access.');
  return;
}

const assert = require('assert');
const path = require('path');
const fs = require('fs');
const repl = require('internal/repl');
const Duplex = require('stream').Duplex;
// Invoking the REPL should create a repl history file at the specified path
// and mode 600.

var stream = new Duplex();
stream.pause = stream.resume = function() {};
// ends immediately
stream._read = function() {
  this.push(null);
};
stream._write = function(c, e, cb) {
  cb();
};
stream.readable = stream.writable = true;

common.refreshTmpDir();
const replHistoryPath = path.join(common.tmpDir, '.node_repl_history');

const checkResults = common.mustCall(function(err, r) {
  if (err)
    throw err;

  // The REPL registers 'module' and 'require' globals
  common.allowGlobals(r.context.module, r.context.require);

  r.input.end();
  const stat = fs.statSync(replHistoryPath);
  assert.strictEqual(
    stat.mode & 0o777, 0o600,
    'REPL history file should be mode 0600');
});

repl.createInternalRepl(
  {NODE_REPL_HISTORY: replHistoryPath},
  {
    terminal: true,
    input: stream,
    output: stream
  },
  checkResults
);
