// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const exec = require('child_process').exec;
let async_completed = 0;
let async_expected = 0;
const unlink = [];
let skipSymlinks = false;
const tmpDir = common.tmpDir;

common.refreshTmpDir();

let root = '/';
let assertEqualPath = assert.strictEqual;
if (common.isWindows) {
  // something like "C:\\"
  root = process.cwd().substr(0, 3);
  assertEqualPath = function(path_left, path_right, message) {
    assert
      .strictEqual(path_left.toLowerCase(), path_right.toLowerCase(), message);
  };

  // On Windows, creating symlinks requires admin privileges.
  // We'll only try to run symlink test if we have enough privileges.
  try {
    exec('whoami /priv', function(err, o) {
      if (err || !o.includes('SeCreateSymbolicLinkPrivilege')) {
        skipSymlinks = true;
      }
      runTest();
    });
  } catch (er) {
    // better safe than sorry
    skipSymlinks = true;
    process.nextTick(runTest);
  }
} else {
  process.nextTick(runTest);
}


function tmp(p) {
  return path.join(tmpDir, p);
}

const targetsAbsDir = path.join(tmpDir, 'targets');
const tmpAbsDir = tmpDir;

// Set up targetsAbsDir and expected subdirectories
fs.mkdirSync(targetsAbsDir);
fs.mkdirSync(path.join(targetsAbsDir, 'nested-index'));
fs.mkdirSync(path.join(targetsAbsDir, 'nested-index', 'one'));
fs.mkdirSync(path.join(targetsAbsDir, 'nested-index', 'two'));

function asynctest(testBlock, args, callback, assertBlock) {
  async_expected++;
  testBlock.apply(testBlock, args.concat(function(err) {
    let ignoreError = false;
    if (assertBlock) {
      try {
        ignoreError = assertBlock.apply(assertBlock, arguments);
      } catch (e) {
        err = e;
      }
    }
    async_completed++;
    callback(ignoreError ? null : err);
  }));
}

// sub-tests:
function test_simple_error_callback(cb) {
  fs.realpath('/this/path/does/not/exist', common.mustCall(function(err, s) {
    assert(err);
    assert(!s);
    cb();
  }));
}

function test_simple_relative_symlink(callback) {
  console.log('test_simple_relative_symlink');
  if (skipSymlinks) {
    common.printSkipMessage('symlink test (no privs)');
    return runNextTest();
  }
  const entry = `${tmpDir}/symlink`;
  const expected = `${tmpDir}/cycles/root.js`;
  [
    [entry, `../${common.tmpDirName}/cycles/root.js`]
  ].forEach(function(t) {
    try { fs.unlinkSync(t[0]); } catch (e) {}
    console.log('fs.symlinkSync(%j, %j, %j)', t[1], t[0], 'file');
    fs.symlinkSync(t[1], t[0], 'file');
    unlink.push(t[0]);
  });
  const result = fs.realpathSync(entry);
  assertEqualPath(result, path.resolve(expected));
  asynctest(fs.realpath, [entry], callback, function(err, result) {
    assertEqualPath(result, path.resolve(expected));
  });
}

function test_simple_absolute_symlink(callback) {
  console.log('test_simple_absolute_symlink');

  // this one should still run, even if skipSymlinks is set,
  // because it uses a junction.
  const type = skipSymlinks ? 'junction' : 'dir';

  console.log('using type=%s', type);

  const entry = `${tmpAbsDir}/symlink`;
  const expected = fixtures.path('nested-index', 'one');
  [
    [entry, expected]
  ].forEach(function(t) {
    try { fs.unlinkSync(t[0]); } catch (e) {}
    console.error('fs.symlinkSync(%j, %j, %j)', t[1], t[0], type);
    fs.symlinkSync(t[1], t[0], type);
    unlink.push(t[0]);
  });
  const result = fs.realpathSync(entry);
  assertEqualPath(result, path.resolve(expected));
  asynctest(fs.realpath, [entry], callback, function(err, result) {
    assertEqualPath(result, path.resolve(expected));
  });
}

function test_deep_relative_file_symlink(callback) {
  console.log('test_deep_relative_file_symlink');
  if (skipSymlinks) {
    common.printSkipMessage('symlink test (no privs)');
    return runNextTest();
  }

  const expected = fixtures.path('cycles', 'root.js');
  const linkData1 = path
                      .relative(path.join(targetsAbsDir, 'nested-index', 'one'),
                                expected);
  const linkPath1 = path.join(targetsAbsDir,
                              'nested-index', 'one', 'symlink1.js');
  try { fs.unlinkSync(linkPath1); } catch (e) {}
  fs.symlinkSync(linkData1, linkPath1, 'file');

  const linkData2 = '../one/symlink1.js';
  const entry = path.join(targetsAbsDir,
                          'nested-index', 'two', 'symlink1-b.js');
  try { fs.unlinkSync(entry); } catch (e) {}
  fs.symlinkSync(linkData2, entry, 'file');
  unlink.push(linkPath1);
  unlink.push(entry);

  assertEqualPath(fs.realpathSync(entry), path.resolve(expected));
  asynctest(fs.realpath, [entry], callback, function(err, result) {
    assertEqualPath(result, path.resolve(expected));
  });
}

function test_deep_relative_dir_symlink(callback) {
  console.log('test_deep_relative_dir_symlink');
  if (skipSymlinks) {
    common.printSkipMessage('symlink test (no privs)');
    return runNextTest();
  }
  const expected = fixtures.path('cycles', 'folder');
  const path1b = path.join(targetsAbsDir, 'nested-index', 'one');
  const linkPath1b = path.join(path1b, 'symlink1-dir');
  const linkData1b = path.relative(path1b, expected);
  try { fs.unlinkSync(linkPath1b); } catch (e) {}
  fs.symlinkSync(linkData1b, linkPath1b, 'dir');

  const linkData2b = '../one/symlink1-dir';
  const entry = path.join(targetsAbsDir,
                          'nested-index', 'two', 'symlink12-dir');
  try { fs.unlinkSync(entry); } catch (e) {}
  fs.symlinkSync(linkData2b, entry, 'dir');
  unlink.push(linkPath1b);
  unlink.push(entry);

  assertEqualPath(fs.realpathSync(entry), path.resolve(expected));

  asynctest(fs.realpath, [entry], callback, function(err, result) {
    assertEqualPath(result, path.resolve(expected));
  });
}

function test_cyclic_link_protection(callback) {
  console.log('test_cyclic_link_protection');
  if (skipSymlinks) {
    common.printSkipMessage('symlink test (no privs)');
    return runNextTest();
  }
  const entry = path.join(tmpDir, '/cycles/realpath-3a');
  [
    [entry, '../cycles/realpath-3b'],
    [path.join(tmpDir, '/cycles/realpath-3b'), '../cycles/realpath-3c'],
    [path.join(tmpDir, '/cycles/realpath-3c'), '../cycles/realpath-3a']
  ].forEach(function(t) {
    try { fs.unlinkSync(t[0]); } catch (e) {}
    fs.symlinkSync(t[1], t[0], 'dir');
    unlink.push(t[0]);
  });
  assert.throws(() => {
    fs.realpathSync(entry);
  }, common.expectsError({ code: 'ELOOP', type: Error }));
  asynctest(
    fs.realpath, [entry], callback, common.mustCall(function(err, result) {
      assert.strictEqual(err.path, entry);
      assert.strictEqual(result, undefined);
      return true;
    }));
}

function test_cyclic_link_overprotection(callback) {
  console.log('test_cyclic_link_overprotection');
  if (skipSymlinks) {
    common.printSkipMessage('symlink test (no privs)');
    return runNextTest();
  }
  const cycles = `${tmpDir}/cycles`;
  const expected = fs.realpathSync(cycles);
  const folder = `${cycles}/folder`;
  const link = `${folder}/cycles`;
  let testPath = cycles;
  testPath += '/folder/cycles'.repeat(10);
  try { fs.unlinkSync(link); } catch (ex) {}
  fs.symlinkSync(cycles, link, 'dir');
  unlink.push(link);
  assertEqualPath(fs.realpathSync(testPath), path.resolve(expected));
  asynctest(fs.realpath, [testPath], callback, function(er, res) {
    assertEqualPath(res, path.resolve(expected));
  });
}

function test_relative_input_cwd(callback) {
  console.log('test_relative_input_cwd');
  if (skipSymlinks) {
    common.printSkipMessage('symlink test (no privs)');
    return runNextTest();
  }

  // we need to calculate the relative path to the tmp dir from cwd
  const entrydir = process.cwd();
  const entry = path.relative(entrydir,
                              path.join(`${tmpDir}/cycles/realpath-3a`));
  const expected = `${tmpDir}/cycles/root.js`;
  [
    [entry, '../cycles/realpath-3b'],
    [`${tmpDir}/cycles/realpath-3b`, '../cycles/realpath-3c'],
    [`${tmpDir}/cycles/realpath-3c`, 'root.js']
  ].forEach(function(t) {
    const fn = t[0];
    console.error('fn=%j', fn);
    try { fs.unlinkSync(fn); } catch (e) {}
    const b = path.basename(t[1]);
    const type = (b === 'root.js' ? 'file' : 'dir');
    console.log('fs.symlinkSync(%j, %j, %j)', t[1], fn, type);
    fs.symlinkSync(t[1], fn, 'file');
    unlink.push(fn);
  });

  const origcwd = process.cwd();
  process.chdir(entrydir);
  assertEqualPath(fs.realpathSync(entry), path.resolve(expected));
  asynctest(fs.realpath, [entry], callback, function(err, result) {
    process.chdir(origcwd);
    assertEqualPath(result, path.resolve(expected));
    return true;
  });
}

function test_deep_symlink_mix(callback) {
  console.log('test_deep_symlink_mix');
  if (common.isWindows) {
    // This one is a mix of files and directories, and it's quite tricky
    // to get the file/dir links sorted out correctly.
    common.printSkipMessage('symlink test (no privs)');
    return runNextTest();
  }

  /*
  /tmp/node-test-realpath-f1 -> $tmpDir/node-test-realpath-d1/foo
  /tmp/node-test-realpath-d1 -> $tmpDir/node-test-realpath-d2
  /tmp/node-test-realpath-d2/foo -> $tmpDir/node-test-realpath-f2
  /tmp/node-test-realpath-f2
    -> $tmpDir/targets/nested-index/one/realpath-c
  $tmpDir/targets/nested-index/one/realpath-c
    -> $tmpDir/targets/nested-index/two/realpath-c
  $tmpDir/targets/nested-index/two/realpath-c -> $tmpDir/cycles/root.js
  $tmpDir/targets/cycles/root.js (hard)
  */
  const entry = tmp('node-test-realpath-f1');
  try { fs.unlinkSync(tmp('node-test-realpath-d2/foo')); } catch (e) {}
  try { fs.rmdirSync(tmp('node-test-realpath-d2')); } catch (e) {}
  fs.mkdirSync(tmp('node-test-realpath-d2'), 0o700);
  try {
    [
      [entry, `${tmpDir}/node-test-realpath-d1/foo`],
      [tmp('node-test-realpath-d1'),
       `${tmpDir}/node-test-realpath-d2`],
      [tmp('node-test-realpath-d2/foo'), '../node-test-realpath-f2'],
      [tmp('node-test-realpath-f2'),
       `${targetsAbsDir}/nested-index/one/realpath-c`],
      [`${targetsAbsDir}/nested-index/one/realpath-c`,
       `${targetsAbsDir}/nested-index/two/realpath-c`],
      [`${targetsAbsDir}/nested-index/two/realpath-c`,
       `${tmpDir}/cycles/root.js`]
    ].forEach(function(t) {
      try { fs.unlinkSync(t[0]); } catch (e) {}
      fs.symlinkSync(t[1], t[0]);
      unlink.push(t[0]);
    });
  } finally {
    unlink.push(tmp('node-test-realpath-d2'));
  }
  const expected = `${tmpAbsDir}/cycles/root.js`;
  assertEqualPath(fs.realpathSync(entry), path.resolve(expected));
  asynctest(fs.realpath, [entry], callback, function(err, result) {
    assertEqualPath(result, path.resolve(expected));
    return true;
  });
}

function test_non_symlinks(callback) {
  console.log('test_non_symlinks');
  const entrydir = path.dirname(tmpAbsDir);
  const entry = `${tmpAbsDir.substr(entrydir.length + 1)}/cycles/root.js`;
  const expected = `${tmpAbsDir}/cycles/root.js`;
  const origcwd = process.cwd();
  process.chdir(entrydir);
  assertEqualPath(fs.realpathSync(entry), path.resolve(expected));
  asynctest(fs.realpath, [entry], callback, function(err, result) {
    process.chdir(origcwd);
    assertEqualPath(result, path.resolve(expected));
    return true;
  });
}

const upone = path.join(process.cwd(), '..');
function test_escape_cwd(cb) {
  console.log('test_escape_cwd');
  asynctest(fs.realpath, ['..'], cb, function(er, uponeActual) {
    assertEqualPath(
      upone, uponeActual,
      `realpath("..") expected: ${path.resolve(upone)} actual:${uponeActual}`);
  });
}
const uponeActual = fs.realpathSync('..');
assertEqualPath(
  upone, uponeActual,
  `realpathSync("..") expected: ${path.resolve(upone)} actual:${uponeActual}`);


// going up with .. multiple times
// .
// `-- a/
//     |-- b/
//     |   `-- e -> ..
//     `-- d -> ..
// realpath(a/b/e/d/a/b/e/d/a) ==> a
function test_up_multiple(cb) {
  console.error('test_up_multiple');
  if (skipSymlinks) {
    common.printSkipMessage('symlink test (no privs)');
    return runNextTest();
  }
  function cleanup() {
    ['a/b',
     'a'
    ].forEach(function(folder) {
      try { fs.rmdirSync(tmp(folder)); } catch (ex) {}
    });
  }
  function setup() {
    cleanup();
  }
  setup();
  fs.mkdirSync(tmp('a'), 0o755);
  fs.mkdirSync(tmp('a/b'), 0o755);
  fs.symlinkSync('..', tmp('a/d'), 'dir');
  unlink.push(tmp('a/d'));
  fs.symlinkSync('..', tmp('a/b/e'), 'dir');
  unlink.push(tmp('a/b/e'));

  const abedabed = tmp('abedabed'.split('').join('/'));
  const abedabed_real = tmp('');

  const abedabeda = tmp('abedabeda'.split('').join('/'));
  const abedabeda_real = tmp('a');

  assertEqualPath(fs.realpathSync(abedabeda), abedabeda_real);
  assertEqualPath(fs.realpathSync(abedabed), abedabed_real);
  fs.realpath(abedabeda, function(er, real) {
    assert.ifError(er);
    assertEqualPath(abedabeda_real, real);
    fs.realpath(abedabed, function(er, real) {
      assert.ifError(er);
      assertEqualPath(abedabed_real, real);
      cb();
      cleanup();
    });
  });
}


// absolute symlinks with children.
// .
// `-- a/
//     |-- b/
//     |   `-- c/
//     |       `-- x.txt
//     `-- link -> /tmp/node-test-realpath-abs-kids/a/b/
// realpath(root+'/a/link/c/x.txt') ==> root+'/a/b/c/x.txt'
function test_abs_with_kids(cb) {
  console.log('test_abs_with_kids');

  // this one should still run, even if skipSymlinks is set,
  // because it uses a junction.
  const type = skipSymlinks ? 'junction' : 'dir';

  console.log('using type=%s', type);

  const root = `${tmpAbsDir}/node-test-realpath-abs-kids`;
  function cleanup() {
    ['/a/b/c/x.txt',
     '/a/link'
    ].forEach(function(file) {
      try { fs.unlinkSync(root + file); } catch (ex) {}
    });
    ['/a/b/c',
     '/a/b',
     '/a',
     ''
    ].forEach(function(folder) {
      try { fs.rmdirSync(root + folder); } catch (ex) {}
    });
  }
  function setup() {
    cleanup();
    ['',
     '/a',
     '/a/b',
     '/a/b/c'
    ].forEach(function(folder) {
      console.log(`mkdir ${root}${folder}`);
      fs.mkdirSync(root + folder, 0o700);
    });
    fs.writeFileSync(`${root}/a/b/c/x.txt`, 'foo');
    fs.symlinkSync(`${root}/a/b`, `${root}/a/link`, type);
  }
  setup();
  const linkPath = `${root}/a/link/c/x.txt`;
  const expectPath = `${root}/a/b/c/x.txt`;
  const actual = fs.realpathSync(linkPath);
  // console.log({link:linkPath,expect:expectPath,actual:actual},'sync');
  assertEqualPath(actual, path.resolve(expectPath));
  asynctest(fs.realpath, [linkPath], cb, function(er, actual) {
    // console.log({link:linkPath,expect:expectPath,actual:actual},'async');
    assertEqualPath(actual, path.resolve(expectPath));
    cleanup();
  });
}

// ----------------------------------------------------------------------------

const tests = [
  test_simple_error_callback,
  test_simple_relative_symlink,
  test_simple_absolute_symlink,
  test_deep_relative_file_symlink,
  test_deep_relative_dir_symlink,
  test_cyclic_link_protection,
  test_cyclic_link_overprotection,
  test_relative_input_cwd,
  test_deep_symlink_mix,
  test_non_symlinks,
  test_escape_cwd,
  test_abs_with_kids,
  test_up_multiple
];
const numtests = tests.length;
let testsRun = 0;
function runNextTest(err) {
  assert.ifError(err);
  const test = tests.shift();
  if (!test) {
    return console.log(`${numtests} subtests completed OK for fs.realpath`);
  }
  testsRun++;
  test(runNextTest);
}


assertEqualPath(root, fs.realpathSync('/'));
fs.realpath('/', function(err, result) {
  assert.ifError(err);
  assertEqualPath(root, result);
});


function runTest() {
  const tmpDirs = ['cycles', 'cycles/folder'];
  tmpDirs.forEach(function(t) {
    t = tmp(t);
    fs.mkdirSync(t, 0o700);
  });
  fs.writeFileSync(tmp('cycles/root.js'), "console.error('roooot!');");
  console.error('start tests');
  runNextTest();
}


process.on('exit', function() {
  assert.strictEqual(numtests, testsRun);
  assert.strictEqual(async_completed, async_expected);
});
