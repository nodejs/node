'use strict';

// Verifies that the REPL history file is created with mode 0600

// Flags: --expose-internals

const common = require('../common');

if (common.isWindows) {
  common.skip('Win32 uses ACLs for file permissions, ' +
              'modes are always 0666 and says nothing about group/other ' +
              'read access.');
}

const assert = require('assert');
const fs = require('fs');
const repl = require('internal/repl');
const Duplex = require('stream').Duplex;
// Invoking the REPL should create a repl history file at the specified path
// and mode 600.

const stream = new Duplex();
stream.pause = stream.resume = () => {};
// ends immediately
stream._read = function() {
  this.push(null);
};
stream._write = function(c, e, cb) {
  cb();
};
stream.readable = stream.writable = true;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const replHistoryPath = tmpdir.resolve('.node_repl_history');

const checkResults = common.mustSucceed((r) => {
  const stat = fs.statSync(replHistoryPath);
  const fileMode = stat.mode & 0o777;
  assert.strictEqual(
    fileMode, 0o600,
    `REPL history file should be mode 0600 but was 0${fileMode.toString(8)}`);

  // Close the REPL
  r.input.emit('keypress', '', { ctrl: true, name: 'd' });
  r.input.end();
});

repl.createInternalRepl(
  { NODE_REPL_HISTORY: replHistoryPath },
  {
    terminal: true,
    input: stream,
    output: stream
  },
  checkResults
);
