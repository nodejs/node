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
require('../common');
const assert = require('assert');
const path = require('path');

const failures = [];
const backslashRE = /\\/g;

const joinTests = [
  [ [path.posix.join, path.win32.join],
    // arguments                     result
    [[['.', 'x/b', '..', '/b/c.js'], 'x/b/c.js'],
     [[], '.'],
     [['/.', 'x/b', '..', '/b/c.js'], '/x/b/c.js'],
     [['/foo', '../../../bar'], '/bar'],
     [['foo', '../../../bar'], '../../bar'],
     [['foo/', '../../../bar'], '../../bar'],
     [['foo/x', '../../../bar'], '../bar'],
     [['foo/x', './bar'], 'foo/x/bar'],
     [['foo/x/', './bar'], 'foo/x/bar'],
     [['foo/x/', '.', 'bar'], 'foo/x/bar'],
     [['./'], './'],
     [['.', './'], './'],
     [['.', '.', '.'], '.'],
     [['.', './', '.'], '.'],
     [['.', '/./', '.'], '.'],
     [['.', '/////./', '.'], '.'],
     [['.'], '.'],
     [['', '.'], '.'],
     [['', 'foo'], 'foo'],
     [['foo', '/bar'], 'foo/bar'],
     [['', '/foo'], '/foo'],
     [['', '', '/foo'], '/foo'],
     [['', '', 'foo'], 'foo'],
     [['foo', ''], 'foo'],
     [['foo/', ''], 'foo/'],
     [['foo', '', '/bar'], 'foo/bar'],
     [['./', '..', '/foo'], '../foo'],
     [['./', '..', '..', '/foo'], '../../foo'],
     [['.', '..', '..', '/foo'], '../../foo'],
     [['', '..', '..', '/foo'], '../../foo'],
     [['/'], '/'],
     [['/', '.'], '/'],
     [['/', '..'], '/'],
     [['/', '..', '..'], '/'],
     [[''], '.'],
     [['', ''], '.'],
     [[' /foo'], ' /foo'],
     [[' ', 'foo'], ' /foo'],
     [[' ', '.'], ' '],
     [[' ', '/'], ' /'],
     [[' ', ''], ' '],
     [['/', 'foo'], '/foo'],
     [['/', '/foo'], '/foo'],
     [['/', '//foo'], '/foo'],
     [['/', '', '/foo'], '/foo'],
     [['', '/', 'foo'], '/foo'],
     [['', '/', '/foo'], '/foo']
    ]
  ]
];

// Windows-specific join tests
joinTests.push([
  path.win32.join,
  joinTests[0][1].slice(0).concat(
    [// arguments                     result
      // UNC path expected
      [['//foo/bar'], '\\\\foo\\bar\\'],
      [['\\/foo/bar'], '\\\\foo\\bar\\'],
      [['\\\\foo/bar'], '\\\\foo\\bar\\'],
      // UNC path expected - server and share separate
      [['//foo', 'bar'], '\\\\foo\\bar\\'],
      [['//foo/', 'bar'], '\\\\foo\\bar\\'],
      [['//foo', '/bar'], '\\\\foo\\bar\\'],
      // UNC path expected - questionable
      [['//foo', '', 'bar'], '\\\\foo\\bar\\'],
      [['//foo/', '', 'bar'], '\\\\foo\\bar\\'],
      [['//foo/', '', '/bar'], '\\\\foo\\bar\\'],
      // UNC path expected - even more questionable
      [['', '//foo', 'bar'], '\\\\foo\\bar\\'],
      [['', '//foo/', 'bar'], '\\\\foo\\bar\\'],
      [['', '//foo/', '/bar'], '\\\\foo\\bar\\'],
      // No UNC path expected (no double slash in first component)
      [['\\', 'foo/bar'], '\\foo\\bar'],
      [['\\', '/foo/bar'], '\\foo\\bar'],
      [['', '/', '/foo/bar'], '\\foo\\bar'],
      // No UNC path expected (no non-slashes in first component -
      // questionable)
      [['//', 'foo/bar'], '\\foo\\bar'],
      [['//', '/foo/bar'], '\\foo\\bar'],
      [['\\\\', '/', '/foo/bar'], '\\foo\\bar'],
      [['//'], '/'],
      // No UNC path expected (share name missing - questionable).
      [['//foo'], '\\foo'],
      [['//foo/'], '\\foo\\'],
      [['//foo', '/'], '\\foo\\'],
      [['//foo', '', '/'], '\\foo\\'],
      // No UNC path expected (too many leading slashes - questionable)
      [['///foo/bar'], '\\foo\\bar'],
      [['////foo', 'bar'], '\\foo\\bar'],
      [['\\\\\\/foo/bar'], '\\foo\\bar'],
      // Drive-relative vs drive-absolute paths. This merely describes the
      // status quo, rather than being obviously right
      [['c:'], 'c:.'],
      [['c:.'], 'c:.'],
      [['c:', ''], 'c:.'],
      [['', 'c:'], 'c:.'],
      [['c:.', '/'], 'c:.\\'],
      [['c:.', 'file'], 'c:file'],
      [['c:', '/'], 'c:\\'],
      [['c:', 'file'], 'c:\\file']
    ]
  )
]);
joinTests.forEach((test) => {
  if (!Array.isArray(test[0]))
    test[0] = [test[0]];
  test[0].forEach((join) => {
    test[1].forEach((test) => {
      const actual = join.apply(null, test[0]);
      const expected = test[1];
      // For non-Windows specific tests with the Windows join(), we need to try
      // replacing the slashes since the non-Windows specific tests' `expected`
      // use forward slashes
      let actualAlt;
      let os;
      if (join === path.win32.join) {
        actualAlt = actual.replace(backslashRE, '/');
        os = 'win32';
      } else {
        os = 'posix';
      }
      const message =
        `path.${os}.join(${test[0].map(JSON.stringify).join(',')})\n  expect=${
          JSON.stringify(expected)}\n  actual=${JSON.stringify(actual)}`;
      if (actual !== expected && actualAlt !== expected)
        failures.push(`\n${message}`);
    });
  });
});
assert.strictEqual(failures.length, 0, failures.join(''));
