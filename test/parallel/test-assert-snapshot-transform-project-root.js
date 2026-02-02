'use strict';

require('../common');
const assert = require('node:assert/strict');
const path = require('node:path');

const snapshot = require('../common/assertSnapshot');

// `transformProjectRoot()` is used by many snapshot-based tests to strip the
// project root from stack traces and other outputs. Ensure it only strips the
// project root when it is a real path prefix, not when it is part of some other
// token (e.g., "node_modules" or URLs).
{
  const stripProjectRoot = snapshot.transformProjectRoot('');
  const projectRoot = path.resolve(__dirname, '..', '..');

  assert.strictEqual(
    stripProjectRoot(`${projectRoot}${path.sep}test${path.sep}fixtures`),
    `${path.sep}test${path.sep}fixtures`,
  );

  const shouldNotStrip = `${projectRoot}_modules`;
  assert.strictEqual(stripProjectRoot(shouldNotStrip), shouldNotStrip);

  const urlLike = `https://${projectRoot}js.org`;
  assert.strictEqual(stripProjectRoot(urlLike), urlLike);

  if (process.platform === 'win32') {
    const projectRootPosix = projectRoot.replaceAll(path.win32.sep, path.posix.sep);

    assert.strictEqual(
      stripProjectRoot(`${projectRootPosix}/test/fixtures`),
      '/test/fixtures',
    );

    const shouldNotStripPosix = `${projectRootPosix}_modules`;
    assert.strictEqual(stripProjectRoot(shouldNotStripPosix), shouldNotStripPosix);
  }
}