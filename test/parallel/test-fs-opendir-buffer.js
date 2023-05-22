'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { opendir } = require('fs/promises');
const path = require('path');

const tmpdir = require('../common/tmpdir');

const testDir = tmpdir.path;
const files = ['empty', 'files', 'for', 'just', 'testing'];

// Make sure tmp directory is clean
tmpdir.refresh();

// Create the necessary files
files.forEach(function(filename) {
  fs.closeSync(fs.openSync(path.join(testDir, filename), 'w'));
});

// Opendir from buffer
{
  (async () => {
    // open the directory
    const dir = await opendir(Buffer.from(testDir), { encoding: 'buffer' });
    const fileNames = [];

    // Iterate and check each files are Buffer
    for await (const dirent of dir) {
      assert.strictEqual(Buffer.isBuffer(dirent.name), true);
      // Convert the buffer filename to string, so that we can ensure all files are returned.
      fileNames.push(dirent.name.toString());
    }

    assert.strictEqual(fileNames.length, files.length);

    // Sort both file arrays, so that it can be compared easily
    assert.deepStrictEqual(fileNames.sort(), files.sort());

  })().then(common.mustCall());
}
