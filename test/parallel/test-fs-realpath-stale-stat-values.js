// Flags: --expose-internals
'use strict';

// Resolving a path must not depend on what was stat'ed before it.
//
// While walking a path, realpath skips the components it already knows are
// real, and in that branch it consulted the shared stat buffer to decide
// whether the walk had reached a pipe or a socket. That buffer holds the result
// of the last stat made anywhere in the process, so an unrelated stat of a FIFO
// made the walk stop early and hand back the path with its symlinks unresolved.
// The unresolved path is then cached, so every later resolution repeats it.

const common = require('../common');

if (common.isWindows)
  common.skip('no mkfifo on Windows');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');
const { realpathCacheKey } = require('internal/fs/utils');
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

// The walk only skips a component once something has established it as real. A
// cache carrying the ancestors is that state, and it is the state the module
// loader's own cache is in after it has resolved anything else under the
// directory.
function ancestorCache() {
  const cache = new Map();
  let dir = '';
  for (const part of tmpdir.path.split(path.sep).slice(1)) {
    dir += path.sep + part;
    cache.set(dir, dir);
  }
  return cache;
}

fs.statSync(path.join(pkg, 'index.js'));
assert.strictEqual(
  fs.realpathSync(throughLink, { [realpathCacheKey]: ancestorCache() }),
  throughReal,
);

fs.statSync(fifo);
assert.strictEqual(
  fs.realpathSync(throughLink, { [realpathCacheKey]: ancestorCache() }),
  throughReal,
);

// What the stale read costs through the module loader, whose cache puts the
// walk in that same state: the symlink stays unresolved, so the file is loaded
// a second time under a second name.
require(tmpdir.resolve('warm.js'));
fs.statSync(fifo);

assert.strictEqual(require.resolve(throughLink), throughReal);
assert.strictEqual(require(throughLink), require(throughReal));
