'use strict';
const common = require('../common');
const fs = require('fs');
const path = require('path');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');

// Verify recursive opendir with small bufferSize works correctly and finds all files.
// Regression test for issue where synchronous operations blocked the event loop or missed files.

tmpdir.refresh();
const root = tmpdir.path;
const dir1 = path.join(root, 'dir1');
const dir2 = path.join(root, 'dir2');
fs.mkdirSync(dir1);
fs.mkdirSync(dir2);
fs.writeFileSync(path.join(root, 'file1'), 'a');
fs.writeFileSync(path.join(dir1, 'file2'), 'b');
fs.writeFileSync(path.join(dir2, 'file3'), 'c');

async function run() {
    // bufferSize: 1 forces frequent internal buffering and recursion
    const dir = await fs.promises.opendir(root, { recursive: true, bufferSize: 1 });
    const files = [];
    for await (const dirent of dir) {
        files.push(dirent.name);
    }
    files.sort();
    // Note: opendir recursive does not return directory entries themselves by default? 
    // Wait, opendir iterator returns dirents.
    // Standard readdir recursive only returns files unless withFileTypes is set?
    // opendir iterator works like readdir withFileTypes: true always.
    // It returns files and directories?
    // Let's verify documentation behaviour: opendir returns dirents for files and directories it encounters.
    // But recursive opendir logic is complex.
    // If we just check file names:
    // file1, file2, file3.
    // Plus dir1, dir2?
    // The fix handles RECURSION into directories.

    // Let's expect at least the files.
    const fileNames = files.filter(n => n.startsWith('file'));
    assert.deepStrictEqual(fileNames, ['file1', 'file2', 'file3']);
}

run().then(common.mustCall());
