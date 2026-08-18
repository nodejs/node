'use strict';

// The async realpath() reads the shared stat buffer the same way realpathSync()
// did, to decide whether the walk has reached a pipe or a socket. The walk's
// own fs.stat() does leave the right value there, but it is not read until
// after fs.readlink() and a process.nextTick(), and any stat completing in that
// window replaces it.
//
// Truncating the walk only costs something when a second symlink follows the
// one being resolved, so the path used here has two.

const common = require('../common');

if (common.isWindows)
  common.skip('no mkfifo on Windows');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const real = tmpdir.resolve('real');
const pkg = tmpdir.resolve('pkg');
const fifo = tmpdir.resolve('fifo');

fs.mkdirSync(real);
fs.mkdirSync(pkg);
fs.writeFileSync(path.join(real, 'index.js'), '');
fs.symlinkSync(path.join('..', 'real'), path.join(pkg, 'sub'));
fs.symlinkSync('pkg', tmpdir.resolve('link'));
execFileSync('mkfifo', [fifo]);

const throughLinks = tmpdir.resolve('link', 'sub', 'index.js');
const expected = path.join(real, 'index.js');

// Keep stats of the FIFO completing for as long as the walk runs, so that one
// of them lands in the buffer during the window.
let settled = false;
(function statFifo() {
  if (settled) return;
  fs.stat(fifo, statFifo);
})();

let error;
let resolvedPath;

fs.realpath(throughLinks, common.mustCall((err, resolved) => {
  settled = true;
  error = err;
  resolvedPath = resolved;
}));

// Asserted on exit rather than in the callback. An assertion that fails inside
// this callback is lost: it does not reach an `uncaughtException` handler and
// the process still exits 0, so the test would pass over the bug it covers.
process.on('exit', () => {
  assert.ifError(error);
  assert.strictEqual(resolvedPath, expected);
});
