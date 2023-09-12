'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const pathModule = require('path');
const tmpdir = require('../common/tmpdir');

const readdirDir = tmpdir.path;

const fileStructure = [
  [ 'a', [ 'a', 'foo', 'bar' ] ],
  [ 'b', [ 'foo', 'bar' ] ],
  [ 'c', [ 'foo', 'bar' ] ],
  [ 'd', [ 'foo', 'bar' ] ],
  [ 'e', [ 'foo', 'bar' ] ],
  [ 'f', [ 'foo', 'bar' ] ],
  [ 'g', [ 'foo', 'bar' ] ],
  [ 'h', [ 'foo', 'bar' ] ],
  [ 'i', [ 'foo', 'bar' ] ],
  [ 'j', [ 'foo', 'bar' ] ],
  [ 'k', [ 'foo', 'bar' ] ],
  [ 'l', [ 'foo', 'bar' ] ],
  [ 'm', [ 'foo', 'bar' ] ],
  [ 'n', [ 'foo', 'bar' ] ],
  [ 'o', [ 'foo', 'bar' ] ],
  [ 'p', [ 'foo', 'bar' ] ],
  [ 'q', [ 'foo', 'bar' ] ],
  [ 'r', [ 'foo', 'bar' ] ],
  [ 's', [ 'foo', 'bar' ] ],
  [ 't', [ 'foo', 'bar' ] ],
  [ 'u', [ 'foo', 'bar' ] ],
  [ 'v', [ 'foo', 'bar' ] ],
  [ 'w', [ 'foo', 'bar' ] ],
  [ 'x', [ 'foo', 'bar' ] ],
  [ 'y', [ 'foo', 'bar' ] ],
  [ 'z', [ 'foo', 'bar' ] ],
  [ 'aa', [ 'foo', 'bar' ] ],
  [ 'bb', [ 'foo', 'bar' ] ],
  [ 'cc', [ 'foo', 'bar' ] ],
  [ 'dd', [ 'foo', 'bar' ] ],
  [ 'ee', [ 'foo', 'bar' ] ],
  [ 'ff', [ 'foo', 'bar' ] ],
  [ 'gg', [ 'foo', 'bar' ] ],
  [ 'hh', [ 'foo', 'bar' ] ],
  [ 'ii', [ 'foo', 'bar' ] ],
  [ 'jj', [ 'foo', 'bar' ] ],
  [ 'kk', [ 'foo', 'bar' ] ],
  [ 'll', [ 'foo', 'bar' ] ],
  [ 'mm', [ 'foo', 'bar' ] ],
  [ 'nn', [ 'foo', 'bar' ] ],
  [ 'oo', [ 'foo', 'bar' ] ],
  [ 'pp', [ 'foo', 'bar' ] ],
  [ 'qq', [ 'foo', 'bar' ] ],
  [ 'rr', [ 'foo', 'bar' ] ],
  [ 'ss', [ 'foo', 'bar' ] ],
  [ 'tt', [ 'foo', 'bar' ] ],
  [ 'uu', [ 'foo', 'bar' ] ],
  [ 'vv', [ 'foo', 'bar' ] ],
  [ 'ww', [ 'foo', 'bar' ] ],
  [ 'xx', [ 'foo', 'bar' ] ],
  [ 'yy', [ 'foo', 'bar' ] ],
  [ 'zz', [ 'foo', 'bar' ] ],
  [ 'abc', [ [ 'def', [ 'foo', [ 'bar', [ 'bim', 'bam' ] ] ] ], [ 'ghi', [ 'foo', [ 'bar', [ 'bim', 'bam' ] ] ] ] ] ],
];

function createFiles(path, fileStructure) {
  for (const fileOrDir of fileStructure) {
    if (typeof fileOrDir === 'string') {
      fs.writeFileSync(pathModule.join(path, fileOrDir), '');
    } else {
      const dirPath = pathModule.join(path, fileOrDir[0]);
      fs.mkdirSync(dirPath);
      createFiles(dirPath, fileOrDir[1]);
    }
  }
}

// Make sure tmp directory is clean
tmpdir.refresh();

const results = [
  [
    'a', 'a/bar', 'a/foo', 'aa', 'aa/bar', 'aa/foo',
    'abc', 'abc/def', 'abc/def/bar', 'abc/def/bar/bam', 'abc/def/bar/bim',
    'abc/def/foo', 'abc/ghi', 'abc/ghi/bar',
    'abc/ghi/bar/bam', 'abc/ghi/bar/bim', 'abc/ghi/foo',
    'b', 'b/bar', 'b/foo', 'bb', 'bb/bar', 'bb/foo',
    'c', 'c/bar', 'c/foo', 'cc', 'cc/bar', 'cc/foo',
    'd', 'd/bar', 'd/foo', 'dd', 'dd/bar', 'dd/foo',
    'e', 'e/bar', 'e/foo', 'ee', 'ee/bar', 'ee/foo',
    'f', 'f/bar', 'f/foo', 'ff', 'ff/bar', 'ff/foo',
    'g', 'g/bar', 'g/foo', 'gg', 'gg/bar', 'gg/foo',
    'h', 'h/bar', 'h/foo', 'hh', 'hh/bar', 'hh/foo',
    'i', 'i/bar', 'i/foo', 'ii', 'ii/bar', 'ii/foo',
    'j', 'j/bar', 'j/foo', 'jj', 'jj/bar', 'jj/foo',
    'k', 'k/bar', 'k/foo', 'kk', 'kk/bar', 'kk/foo',
    'l', 'l/bar', 'l/foo', 'll', 'll/bar', 'll/foo',
    'm', 'm/bar', 'm/foo', 'mm', 'mm/bar', 'mm/foo',
    'n', 'n/bar', 'n/foo', 'nn', 'nn/bar', 'nn/foo',
    'o', 'o/bar', 'o/foo', 'oo', 'oo/bar', 'oo/foo',
    'p', 'p/bar', 'p/foo', 'pp', 'pp/bar', 'pp/foo',
    'q', 'q/bar', 'q/foo', 'qq', 'qq/bar', 'qq/foo',
    'r', 'r/bar', 'r/foo', 'rr', 'rr/bar', 'rr/foo',
    's', 's/bar', 's/foo', 'ss', 'ss/bar', 'ss/foo',
    'symlinks', 'symlinks/symlink-src-dir', 'symlinks/symlink-src-file',
    'symlinks/symlink-target-dir', 'symlinks/symlink-target-file',
    't', 't/bar', 't/foo', 'tt', 'tt/bar', 'tt/foo',
    'u', 'u/bar', 'u/foo', 'uu', 'uu/bar', 'uu/foo',
    'v', 'v/bar', 'v/foo', 'vv', 'vv/bar', 'vv/foo',
    'w', 'w/bar', 'w/foo', 'ww', 'ww/bar', 'ww/foo',
    'x', 'x/bar', 'x/foo', 'xx', 'xx/bar', 'xx/foo',
    'y', 'y/bar', 'y/foo', 'yy', 'yy/bar', 'yy/foo',
    'z', 'z/bar', 'z/foo', 'zz', 'zz/bar', 'zz/foo',
  ],
  [
    'a', 'a/bar', 'a/foo', 'aa', 'aa/bar', 'aa/foo',
    'abc', 'abc/def', 'abc/ghi',
    'b', 'b/bar', 'b/foo', 'bb', 'bb/bar', 'bb/foo',
    'c', 'c/bar', 'c/foo', 'cc', 'cc/bar', 'cc/foo',
    'd', 'd/bar', 'd/foo', 'dd', 'dd/bar', 'dd/foo',
    'e', 'e/bar', 'e/foo', 'ee', 'ee/bar', 'ee/foo',
    'f', 'f/bar', 'f/foo', 'ff', 'ff/bar', 'ff/foo',
    'g', 'g/bar', 'g/foo', 'gg', 'gg/bar', 'gg/foo',
    'h', 'h/bar', 'h/foo', 'hh', 'hh/bar', 'hh/foo',
    'i', 'i/bar', 'i/foo', 'ii', 'ii/bar', 'ii/foo',
    'j', 'j/bar', 'j/foo', 'jj', 'jj/bar', 'jj/foo',
    'k', 'k/bar', 'k/foo', 'kk', 'kk/bar', 'kk/foo',
    'l', 'l/bar', 'l/foo', 'll', 'll/bar', 'll/foo',
    'm', 'm/bar', 'm/foo', 'mm', 'mm/bar', 'mm/foo',
    'n', 'n/bar', 'n/foo', 'nn', 'nn/bar', 'nn/foo',
    'o', 'o/bar', 'o/foo', 'oo', 'oo/bar', 'oo/foo',
    'p', 'p/bar', 'p/foo', 'pp', 'pp/bar', 'pp/foo',
    'q', 'q/bar', 'q/foo', 'qq', 'qq/bar', 'qq/foo',
    'r', 'r/bar', 'r/foo', 'rr', 'rr/bar', 'rr/foo',
    's', 's/bar', 's/foo', 'ss', 'ss/bar', 'ss/foo',
    'symlinks', 'symlinks/symlink-src-dir', 'symlinks/symlink-src-file',
    'symlinks/symlink-target-dir', 'symlinks/symlink-target-file',
    't', 't/bar', 't/foo', 'tt', 'tt/bar', 'tt/foo',
    'u', 'u/bar', 'u/foo', 'uu', 'uu/bar', 'uu/foo',
    'v', 'v/bar', 'v/foo', 'vv', 'vv/bar', 'vv/foo',
    'w', 'w/bar', 'w/foo', 'ww', 'ww/bar', 'ww/foo',
    'x', 'x/bar', 'x/foo', 'xx', 'xx/bar', 'xx/foo',
    'y', 'y/bar', 'y/foo', 'yy', 'yy/bar', 'yy/foo',
    'z', 'z/bar', 'z/foo', 'zz', 'zz/bar', 'zz/foo',
  ],
  [
    'a', 'a/bar', 'a/foo', 'aa', 'aa/bar', 'aa/foo',
    'abc', 'abc/def', 'abc/def/bar', 'abc/def/foo',
    'abc/ghi', 'abc/ghi/bar', 'abc/ghi/foo',
    'b', 'b/bar', 'b/foo', 'bb', 'bb/bar', 'bb/foo',
    'c', 'c/bar', 'c/foo', 'cc', 'cc/bar', 'cc/foo',
    'd', 'd/bar', 'd/foo', 'dd', 'dd/bar', 'dd/foo',
    'e', 'e/bar', 'e/foo', 'ee', 'ee/bar', 'ee/foo',
    'f', 'f/bar', 'f/foo', 'ff', 'ff/bar', 'ff/foo',
    'g', 'g/bar', 'g/foo', 'gg', 'gg/bar', 'gg/foo',
    'h', 'h/bar', 'h/foo', 'hh', 'hh/bar', 'hh/foo',
    'i', 'i/bar', 'i/foo', 'ii', 'ii/bar', 'ii/foo',
    'j', 'j/bar', 'j/foo', 'jj', 'jj/bar', 'jj/foo',
    'k', 'k/bar', 'k/foo', 'kk', 'kk/bar', 'kk/foo',
    'l', 'l/bar', 'l/foo', 'll', 'll/bar', 'll/foo',
    'm', 'm/bar', 'm/foo', 'mm', 'mm/bar', 'mm/foo',
    'n', 'n/bar', 'n/foo', 'nn', 'nn/bar', 'nn/foo',
    'o', 'o/bar', 'o/foo', 'oo', 'oo/bar', 'oo/foo',
    'p', 'p/bar', 'p/foo', 'pp', 'pp/bar', 'pp/foo',
    'q', 'q/bar', 'q/foo', 'qq', 'qq/bar', 'qq/foo',
    'r', 'r/bar', 'r/foo', 'rr', 'rr/bar', 'rr/foo',
    's', 's/bar', 's/foo', 'ss', 'ss/bar', 'ss/foo',
    'symlinks', 'symlinks/symlink-src-dir', 'symlinks/symlink-src-file',
    'symlinks/symlink-target-dir', 'symlinks/symlink-target-file',
    't', 't/bar', 't/foo', 'tt', 'tt/bar', 'tt/foo',
    'u', 'u/bar', 'u/foo', 'uu', 'uu/bar', 'uu/foo',
    'v', 'v/bar', 'v/foo', 'vv', 'vv/bar', 'vv/foo',
    'w', 'w/bar', 'w/foo', 'ww', 'ww/bar', 'ww/foo',
    'x', 'x/bar', 'x/foo', 'xx', 'xx/bar', 'xx/foo',
    'y', 'y/bar', 'y/foo', 'yy', 'yy/bar', 'yy/foo',
    'z', 'z/bar', 'z/foo', 'zz', 'zz/bar', 'zz/foo',
  ],
  [
    'a', 'a/bar', 'a/foo', 'aa', 'aa/bar', 'aa/foo',
    'abc', 'abc/def', 'abc/def/bar',
    'abc/def/foo', 'abc/ghi',
    'b', 'b/bar', 'b/foo', 'bb', 'bb/bar', 'bb/foo',
    'c', 'c/bar', 'c/foo', 'cc', 'cc/bar', 'cc/foo',
    'd', 'd/bar', 'd/foo', 'dd', 'dd/bar', 'dd/foo',
    'e', 'e/bar', 'e/foo', 'ee', 'ee/bar', 'ee/foo',
    'f', 'f/bar', 'f/foo', 'ff', 'ff/bar', 'ff/foo',
    'g', 'g/bar', 'g/foo', 'gg', 'gg/bar', 'gg/foo',
    'h', 'h/bar', 'h/foo', 'hh', 'hh/bar', 'hh/foo',
    'i', 'i/bar', 'i/foo', 'ii', 'ii/bar', 'ii/foo',
    'j', 'j/bar', 'j/foo', 'jj', 'jj/bar', 'jj/foo',
    'k', 'k/bar', 'k/foo', 'kk', 'kk/bar', 'kk/foo',
    'l', 'l/bar', 'l/foo', 'll', 'll/bar', 'll/foo',
    'm', 'm/bar', 'm/foo', 'mm', 'mm/bar', 'mm/foo',
    'n', 'n/bar', 'n/foo', 'nn', 'nn/bar', 'nn/foo',
    'o', 'o/bar', 'o/foo', 'oo', 'oo/bar', 'oo/foo',
    'p', 'p/bar', 'p/foo', 'pp', 'pp/bar', 'pp/foo',
    'q', 'q/bar', 'q/foo', 'qq', 'qq/bar', 'qq/foo',
    'r', 'r/bar', 'r/foo', 'rr', 'rr/bar', 'rr/foo',
    's', 's/bar', 's/foo', 'ss', 'ss/bar', 'ss/foo',
    'symlinks', 'symlinks/symlink-src-dir', 'symlinks/symlink-src-file',
    'symlinks/symlink-target-dir', 'symlinks/symlink-target-file',
    't', 't/bar', 't/foo', 'tt', 'tt/bar', 'tt/foo',
    'u', 'u/bar', 'u/foo', 'uu', 'uu/bar', 'uu/foo',
    'v', 'v/bar', 'v/foo', 'vv', 'vv/bar', 'vv/foo',
    'w', 'w/bar', 'w/foo', 'ww', 'ww/bar', 'ww/foo',
    'x', 'x/bar', 'x/foo', 'xx', 'xx/bar', 'xx/foo',
    'y', 'y/bar', 'y/foo', 'yy', 'yy/bar', 'yy/foo',
    'z', 'z/bar', 'z/foo', 'zz', 'zz/bar', 'zz/foo',
  ],
];

createFiles(readdirDir, fileStructure);
const symlinksRootPath = pathModule.join(readdirDir, 'symlinks');
const symlinkTargetFile = pathModule.join(symlinksRootPath, 'symlink-target-file');
const symlinkTargetDir = pathModule.join(symlinksRootPath, 'symlink-target-dir');
fs.mkdirSync(symlinksRootPath);
fs.writeFileSync(symlinkTargetFile, '');
fs.mkdirSync(symlinkTargetDir);
if (process.platform === 'win32') {
  results.forEach(function(value) {
    value.splice(value.indexOf('symlinks/symlink-src-dir'), 1);
    value.splice(value.indexOf('symlinks/symlink-src-file'), 1);
  });
} else {
  fs.symlinkSync(symlinkTargetFile, pathModule.join(symlinksRootPath, 'symlink-src-file'));
  fs.symlinkSync(symlinkTargetDir, pathModule.join(symlinksRootPath, 'symlink-src-dir'));
}

// Normalize paths once for non POSIX platforms
results.forEach(function(value) {
  for (let i = 0; i < value.length; i++) {
    value[i] = pathModule.normalize(value[i]);
  }
});

function getDirentPath(dirent) {
  return pathModule.relative(readdirDir, pathModule.join(dirent.path, dirent.name));
}

function assertDirents(dirents, index) {
  const testCase = results[index];
  assert.strictEqual(dirents.length, testCase.length);
  dirents.sort((a, b) => (getDirentPath(a) < getDirentPath(b) ? -1 : 1));
  for (const [i, dirent] of dirents.entries()) {
    assert(dirent instanceof fs.Dirent);
    assert.notStrictEqual(dirent.name, undefined);
    assert.strictEqual(getDirentPath(dirent), testCase[i]);
  }
}

// readdirSync

// readdirSync { recursive }
{
  const result = fs.readdirSync(readdirDir, { recursive: true });
  assert.deepStrictEqual(result.sort(), results[0]);
}

// readdirSync { recursive: depth 1 }
{
  const result = fs.readdirSync(readdirDir, {
    recursive: function(name, depth) {
      return depth < 1;
    }
  });
  assert.deepStrictEqual(result.sort(), results[1]);
}

// readdirSync { recursive: depth 2 }
{
  const result = fs.readdirSync(readdirDir, {
    recursive: function(name, depth) {
      return depth < 2;
    }
  });
  assert.deepStrictEqual(result.sort(), results[2]);
}

// readdirSync { recursive: name 'def' }
{
  const result = fs.readdirSync(readdirDir, {
    recursive: function(name, depth) {
      return depth < 1 || (name.includes('def') && depth > 0);
    }
  });
  assert.deepStrictEqual(result.sort(), results[3]);
}

// readdirSync { recursive, withFileTypes }
{
  const result = fs.readdirSync(readdirDir, {
    recursive: true,
    withFileTypes: true
  });
  assertDirents(result, 0);
}

// readdirSync { recursive: depth 1, withFileTypes }
{
  const result = fs.readdirSync(readdirDir, {
    recursive: function(dirent, depth) {
      return depth < 1;
    },
    withFileTypes: true
  });
  assertDirents(result, 1);
}

// readdirSync { recursive: depth 2, withFileTypes }
{
  const result = fs.readdirSync(readdirDir, {
    recursive: function(dirent, depth) {
      return depth < 2;
    },
    withFileTypes: true
  });
  assertDirents(result, 2);
}

// readdirSync { recursive: name 'def', withFileTypes }
{
  const result = fs.readdirSync(readdirDir, {
    recursive: function(dirent, depth) {
      return depth < 1 || (dirent.name.includes('def') && depth > 0);
    },
    withFileTypes: true
  });
  assertDirents(result, 3);
}

// readdir

// Readdir { recursive } callback
{
  fs.readdir(readdirDir, { recursive: true },
             common.mustSucceed((result) => {
               assert.deepStrictEqual(result.sort(), results[0]);
             }));
}

// Readdir { recursive: depth 1 } callback
{
  fs.readdir(readdirDir, {
    recursive: function(name, depth) {
      return depth < 1;
    }
  },
             common.mustSucceed((result) => {
               assert.deepStrictEqual(result.sort(), results[1]);
             }));
}

// Readdir { recursive: depth 2 } callback
{
  fs.readdir(readdirDir, {
    recursive: function(name, depth) {
      return depth < 2;
    }
  },
             common.mustSucceed((result) => {
               assert.deepStrictEqual(result.sort(), results[2]);
             }));
}

// Readdir { recursive: name 'def' } callback
{
  fs.readdir(readdirDir, {
    recursive: function(name, depth) {
      return depth < 1 || (name.includes('def') && depth > 0);
    }
  },
             common.mustSucceed((result) => {
               assert.deepStrictEqual(result.sort(), results[3]);
             }));
}

// Readdir { recursive, withFileTypes } callback
{
  fs.readdir(readdirDir, {
    recursive: true,
    withFileTypes: true
  },
             common.mustSucceed((result) => {
               assertDirents(result, 0);
             }));
}

// Readdir { recursive: depth 1, withFileTypes } callback
{
  fs.readdir(readdirDir, {
    recursive: function(dirent, depth) {
      return depth < 1;
    },
    withFileTypes: true
  },
             common.mustSucceed((result) => {
               assertDirents(result, 1);
             }));
}

// Readdir { recursive: depth 2, withFileTypes } callback
{
  fs.readdir(readdirDir, {
    recursive: function(dirent, depth) {
      return depth < 2;
    },
    withFileTypes: true
  },
             common.mustSucceed((result) => {
               assertDirents(result, 2);
             }));
}

// Readdir { recursive: name 'def', withFileTypes } callback
{
  fs.readdir(readdirDir, {
    recursive: function(dirent, depth) {
      return depth < 1 || (dirent.name.includes('def') && depth > 0);
    },
    withFileTypes: true
  },
             common.mustSucceed((result) => {
               assertDirents(result, 3);
             }));
}

// fs.promises.readdir

// fs.promises.readdir { recursive }
{
  async function test() {
    const result = await fs.promises.readdir(readdirDir, { recursive: true });
    assert.deepStrictEqual(result.sort(), results[0]);
  }

  test().then(common.mustCall());
}

// fs.promises.readdir { recursive: depth 1 }
{
  async function test() {
    const result = await fs.promises.readdir(readdirDir, {
      recursive: function(name, depth) {
        return depth < 1;
      }
    });
    assert.deepStrictEqual(result.sort(), results[1]);
  }

  test().then(common.mustCall());
}

// fs.promises.readdir { recursive: depth 2 }
{
  async function test() {
    const result = await fs.promises.readdir(readdirDir, {
      recursive: function(name, depth) {
        return depth < 2;
      }
    });
    assert.deepStrictEqual(result.sort(), results[2]);
  }

  test().then(common.mustCall());
}

// fs.promises.readdir { recursive: name 'def' }
{
  async function test() {
    const result = await fs.promises.readdir(readdirDir, {
      recursive: function(name, depth) {
        return depth < 1 || (name.includes('def') && depth > 0);
      }
    });
    assert.deepStrictEqual(result.sort(), results[3]);
  }

  test().then(common.mustCall());
}

// fs.promises.readdir { recursive, withFileTypes }
{
  async function test() {
    const result = await fs.promises.readdir(readdirDir, {
      recursive: true,
      withFileTypes: true
    });
    assertDirents(result, 0);
  }

  test().then(common.mustCall());
}

// fs.promises.readdir { recursive: depth 1, withFileTypes }
{
  async function test() {
    const result = await fs.promises.readdir(readdirDir, {
      recursive: function(dirent, depth) {
        return depth < 1;
      },
      withFileTypes: true
    });
    assertDirents(result, 1);
  }

  test().then(common.mustCall());
}

// fs.promises.readdir { recursive: depth 2, withFileTypes }
{
  async function test() {
    const result = await fs.promises.readdir(readdirDir, {
      recursive: function(dirent, depth) {
        return depth < 2;
      },
      withFileTypes: true
    });
    assertDirents(result, 2);
  }

  test().then(common.mustCall());
}

// fs.promises.readdir { recursive: name 'def', withFileTypes }
{
  async function test() {
    const result = await fs.promises.readdir(readdirDir, {
      recursive: function(dirent, depth) {
        return depth < 1 || (dirent.name.includes('def') && depth > 0);
      },
      withFileTypes: true
    });
    assertDirents(result, 3);
  }

  test().then(common.mustCall());
}
