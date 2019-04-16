// Flags: --expose-internals
'use strict';

const assert = require('assert');
const common = require('../common');
const fs = require('internal/fs/utils');
const pathModule = require('path');

// Valid encodings and no args should not throw.
fs.assertEncoding();
fs.assertEncoding('utf8');

common.expectsError(
  () => fs.assertEncoding('foo'),
  { code: 'ERR_INVALID_OPT_VALUE_ENCODING', type: TypeError }
);

{
  // Enable monkey patching for

  // 1. path.resolve
  const originalResolve = pathModule.resolve;

  // 2. path.toNamespacedPath
  const originalToNamespacedPath = pathModule.toNamespacedPath;

  // 3. process.platform
  const originalPlatform = process.platform;
  let platform = null;
  Object.defineProperty(process, 'platform', { get: () => platform });

  function test(newPlatform, pathString, linkPath) {
    platform = newPlatform;
    pathModule.resolve = pathModule[newPlatform].resolve;
    pathModule.toNamespacedPath = pathModule[newPlatform].toNamespacedPath;

    const preprocessSymlinkDestination = fs.preprocessSymlinkDestination(
      pathString,
      'junction',
      linkPath
    );

    const constructedPath = pathModule[newPlatform].toNamespacedPath(
      pathModule[newPlatform].resolve(linkPath, '..', pathString)
    );

    assert.strictEqual(preprocessSymlinkDestination, constructedPath);
  }

  test('win32', '\\test2', '\\test1');
  test('posix', '\\test2', '\\test1');

  platform = originalPlatform;
  pathModule.resolve = originalResolve;
  pathModule.toNamespacedPath = originalToNamespacedPath;
}
