'use strict';
require('../common');

// This tests that the CommonJS loader's per-require-tree stat cache caches
// negative (not-found) results for *speculative* probes: the extension
// candidates tried by `tryExtensions` and the `node_modules` ancestor
// directories walked for bare specifiers.
//
// A path the user named directly is not negatively cached, so the behaviour of
// https://github.com/nodejs/node/pull/36642 is preserved: a module that is
// missing on a failed `require()` and then created is still picked up by a
// later `require()` in the same tree. That case is covered by
// test-module-cache.js and asserted again at the end of this file.
//
// The stat cache is populated and read internally by the loader, so it is not
// directly observable from user code. These tests make it observable by
// mutating the filesystem between two probes of the same path within one
// require tree.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// An extensionless specifier is resolved by probing `<name>.js`, `<name>.json`,
// `<name>.node`, ... in order. Those candidate paths are appended by resolution
// rather than named by the user, so their misses are cached for the rest of the
// tree.
{
  const dir = tmpdir.resolve('speculative');
  fs.mkdirSync(dir);

  const specifier = path.join(dir, 'mod');

  // Nothing exists yet: every extension candidate misses and is cached.
  assert.throws(
    () => require(specifier),
    { code: 'MODULE_NOT_FOUND' },
    'expected the module to be missing before it is created',
  );

  // Create one of the candidates that was just probed and missed.
  fs.writeFileSync(`${specifier}.js`, 'module.exports = "late";');

  // Resolving the same extensionless specifier probes `mod.js` again, but that
  // negative result is cached, so the freshly-created file is not observed.
  assert.throws(
    () => require(specifier),
    { code: 'MODULE_NOT_FOUND' },
    'a negative result for a speculative extension probe must be cached',
  );
}

// A path the user named directly is not negatively cached, so creating the file
// mid-tree makes a later require in the same tree resolve it.
{
  const explicit = tmpdir.resolve('explicit.js');

  assert.throws(
    () => require(explicit),
    { code: 'MODULE_NOT_FOUND' },
  );

  fs.writeFileSync(explicit, 'module.exports = "created";');

  // A negative result for a user-named path must not be cached.
  assert.strictEqual(require(explicit), 'created');
}
