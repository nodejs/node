'use strict';
require('../common');
const assert = require('assert');
const path = require('path');

const failures = [];
const backslashRE = /\\/g;

const joinTests = [
  [ [path.posix.join, path.win32.join],
    // Arguments                     result
    [[[Buffer.from('.'), Buffer.from('x/b'), '..', Buffer.from('/b/c.js')], 'x/b/c.js'],
     [[], '.'],
     [[Buffer.from('/.'), 'x/b', '..', '/b/c.js'], '/x/b/c.js'],
     [['/foo', Buffer.from('../../../bar')], '/bar'],
     [['foo', '../../../bar'], '../../bar'],
     [['/', Buffer.from(''), '/foo'], '/foo'],
     [['', '/', Buffer.from('foo')], '/foo'],
     [['', '/', '/foo'], '/foo']
    ]
  ]
];

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
      if (actual !== expected && actualAlt !== expected) {
        const delimiter = test[0].map(JSON.stringify).join(',');
        const message = `path.${os}.join(${delimiter})\n  expect=${
          JSON.stringify(expected)}\n  actual=${JSON.stringify(actual)}`;
        failures.push(`\n${message}`);
      }
    });
  });
});
assert.strictEqual(failures.length, 0, failures.join(''));
