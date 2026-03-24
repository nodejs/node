'use strict';

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Test readFileSync with flag: 'w+' truncates existing file
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'original content');

  // Reading with 'w+' flag should truncate then read (empty result)
  const result = myVfs.readFileSync('/file.txt', { flag: 'w+' });
  assert.strictEqual(result.length, 0);
}

// Test readFileSync with flag: 'a+' on new file
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir', { recursive: true });

  // Reading with 'a+' flag should create file if missing and return empty
  const result = myVfs.readFileSync('/dir/new.txt', { flag: 'a+' });
  assert.strictEqual(result.length, 0);

  // File should now exist
  assert.strictEqual(myVfs.existsSync('/dir/new.txt'), true);
}

// Test async readFile with flag: 'w+' truncates existing file
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file2.txt', 'some data');

  myVfs.promises.readFile('/file2.txt', { flag: 'w+' }).then(common.mustCall((result) => {
    assert.strictEqual(result.length, 0);
  }));
}
