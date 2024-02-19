'use strict';

const fs = require('node:fs');
const path = require('path');
const assert = require('assert/strict');
const { describe, it } = require('node:test');

describe('File operations with filenames containing surrogate pairs', () => {
  it('should write, read, and delete a file with surrogate pairs in the filename', () => {
    // Create a temporary directory
    const tempdir = fs.mkdtempSync('emoji-fruit-🍇 🍈 🍉 🍊 🍋');
    assert.strictEqual(fs.existsSync(tempdir), true);

    const filename = '🚀🔥🛸.txt';
    const content = 'Test content';

    // Write content to a file
    fs.writeFileSync(path.join(tempdir, filename), content);

    // Read content from the file
    const readContent = fs.readFileSync(path.join(tempdir, filename), 'utf8');

    // Check if the content matches
    assert.strictEqual(readContent, content);

    // Get directory contents
    const dirs = fs.readdirSync(tempdir);
    assert.strictEqual(dirs.length > 0, true);

    // Check if the file is in the directory contents
    let match = false;
    for (let i = 0; i < dirs.length; i++) {
      if (dirs[i].endsWith(filename)) {
        match = true;
        break;
      }
    }
    assert.strictEqual(match, true);

    // Delete the file
    fs.unlinkSync(path.join(tempdir, filename));
    assert.strictEqual(fs.existsSync(path.join(tempdir, filename)), false);

    // Remove the temporary directory
    fs.rmdirSync(tempdir);
    assert.strictEqual(fs.existsSync(tempdir), false);
  });
});
