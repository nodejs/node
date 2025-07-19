// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

// This test verifies that fs.promises.mkdtemp() returns a proper rejected promise
// when permissions are denied, instead of throwing a TypeError
{
  // Test with simple string path (reproduces the exact issue from GitHub)
  assert.rejects(
    fs.promises.mkdtemp('test'),
    {
      code: 'ERR_ACCESS_DENIED',
      permission: 'FileSystemWrite',
    }
  ).then(common.mustCall());
}