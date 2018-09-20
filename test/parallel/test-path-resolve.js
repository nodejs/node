'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const child = require('child_process');
const path = require('path');

const failures = [];
const slashRE = /\//g;
const backslashRE = /\\/g;

const resolveTests = [
  [ path.win32.resolve,
    // arguments                               result
    [[['c:/blah\\blah', 'd:/games', 'c:../a'], 'c:\\blah\\a'],
     [['c:/ignore', 'd:\\a/b\\c/d', '\\e.exe'], 'd:\\e.exe'],
     [['c:/ignore', 'c:/some/file'], 'c:\\some\\file'],
     [['d:/ignore', 'd:some/dir//'], 'd:\\ignore\\some\\dir'],
     [['.'], process.cwd()],
     [['//server/share', '..', 'relative\\'], '\\\\server\\share\\relative'],
     [['c:/', '//'], 'c:\\'],
     [['c:/', '//dir'], 'c:\\dir'],
     [['c:/', '//server/share'], '\\\\server\\share\\'],
     [['c:/', '//server//share'], '\\\\server\\share\\'],
     [['c:/', '///some//dir'], 'c:\\some\\dir'],
     [['C:\\foo\\tmp.3\\', '..\\tmp.3\\cycles\\root.js'],
      'C:\\foo\\tmp.3\\cycles\\root.js']
    ]
  ],
  [ path.posix.resolve,
    // arguments                    result
    [[['/var/lib', '../', 'file/'], '/var/file'],
     [['/var/lib', '/../', 'file/'], '/file'],
     [['a/b/c/', '../../..'], process.cwd()],
     [['.'], process.cwd()],
     [['/some/dir', '.', '/absolute/'], '/absolute'],
     [['/foo/tmp.3/', '../tmp.3/cycles/root.js'], '/foo/tmp.3/cycles/root.js']
    ]
  ]
];
resolveTests.forEach((test) => {
  const resolve = test[0];
  test[1].forEach((test) => {
    const actual = resolve.apply(null, test[0]);
    let actualAlt;
    const os = resolve === path.win32.resolve ? 'win32' : 'posix';
    if (resolve === path.win32.resolve && !common.isWindows)
      actualAlt = actual.replace(backslashRE, '/');
    else if (resolve !== path.win32.resolve && common.isWindows)
      actualAlt = actual.replace(slashRE, '\\');

    const expected = test[1];
    const message =
      `path.${os}.resolve(${test[0].map(JSON.stringify).join(',')})\n  expect=${
        JSON.stringify(expected)}\n  actual=${JSON.stringify(actual)}`;
    if (actual !== expected && actualAlt !== expected)
      failures.push(`\n${message}`);
  });
});
assert.strictEqual(failures.length, 0, failures.join(''));

if (common.isWindows) {
  // Test resolving the current Windows drive letter from a spawned process.
  // See https://github.com/nodejs/node/issues/7215
  const currentDriveLetter = path.parse(process.cwd()).root.substring(0, 2);
  const resolveFixture = fixtures.path('path-resolve.js');
  const spawnResult = child.spawnSync(
    process.argv[0], [resolveFixture, currentDriveLetter]);
  const resolvedPath = spawnResult.stdout.toString().trim();
  assert.strictEqual(resolvedPath.toLowerCase(), process.cwd().toLowerCase());
}
