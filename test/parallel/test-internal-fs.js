// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const fs = require('internal/fs/utils');

// Valid encodings and no args should not throw.
fs.assertEncoding();
fs.assertEncoding('utf8');

assert.throws(
  () => fs.assertEncoding('foo'),
  { code: 'ERR_INVALID_OPT_VALUE_ENCODING', name: 'TypeError' }
);

// Test junction symlinks
{
  const pathString = 'c:\\test1';
  const linkPathString = '\\test2';

  const preprocessSymlinkDestination = fs.preprocessSymlinkDestination(
    pathString,
    'junction',
    linkPathString
  );

  if (process.platform === 'win32') {
    assert.strictEqual(/^\\\\\?\\/.test(preprocessSymlinkDestination), true);
  } else {
    assert.strictEqual(preprocessSymlinkDestination, pathString);
  }
}

// Test none junction symlinks
{
  const pathString = 'c:\\test1';
  const linkPathString = '\\test2';

  const preprocessSymlinkDestination = fs.preprocessSymlinkDestination(
    pathString,
    undefined,
    linkPathString
  );

  if (process.platform === 'win32') {
    // There should not be any forward slashes
    assert.strictEqual(
      /\//.test(preprocessSymlinkDestination), false);
  } else {
    assert.strictEqual(preprocessSymlinkDestination, pathString);
  }
}
