'use strict';

// Resolving a path must not depend on what was stat'ed before it.
//
// While walking a path, realpath skips the components it already knows are
// real, and in that branch it consulted the shared stat buffer to decide
// whether the walk had reached a pipe or a socket. That buffer holds the result
// of the last stat made anywhere in the process, so an unrelated stat of a FIFO
// made the walk stop early and hand back the path with its symlinks unresolved.
// The unresolved path is then cached, so every later resolution repeats it.
//
// The walk only takes that branch once something has established the ancestors
// as real, which is the state the module loader's realpath cache is in after it
// has resolved anything else under the same directory. So this goes through
// require() to reach it, and the second copy of the module is what the stale
// read costs.

const common = require('../common');

if (common.isWindows)
  common.skip('no mkfifo on Windows');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const pkg = tmpdir.resolve('pkg');
const link = tmpdir.resolve('pkg-link');
const fifo = tmpdir.resolve('fifo');

fs.mkdirSync(pkg);
fs.writeFileSync(path.join(pkg, 'index.js'), 'module.exports = {};\n');
fs.writeFileSync(tmpdir.resolve('warm.js'), 'module.exports = {};\n');
fs.symlinkSync('pkg', link);
execFileSync('mkfifo', [fifo]);

const throughLink = path.join(link, 'index.js');
const throughReal = path.join(pkg, 'index.js');

require(tmpdir.resolve('warm.js'));
fs.statSync(fifo);

assert.strictEqual(require.resolve(throughLink), throughReal);
assert.strictEqual(require(throughLink), require(throughReal));
