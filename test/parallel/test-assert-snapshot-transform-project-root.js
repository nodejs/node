'use strict';

const assert = require('node:assert/strict');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

const snapshot = require('../common/assertSnapshot');

// Coverage: boundary strip, URL tokens, file:// root, % encoding.
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

  const shouldNotStripSuffix = `${projectRoot}suffix/test`;
  assert.strictEqual(stripProjectRoot(shouldNotStripSuffix), shouldNotStripSuffix);

  const fileUrl = pathToFileURL(projectRoot).href;
  assert.strictEqual(stripProjectRoot(`${fileUrl}/test/fixtures`), 'file:///test/fixtures');

  const fileUrlWithSpace = pathToFileURL(path.join(projectRoot, 'spaced dir')).href;
  assert.strictEqual(stripProjectRoot(fileUrlWithSpace), 'file:///spaced%20dir');

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
