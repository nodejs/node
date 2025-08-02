'use strict';

require('../common');
const assert = require('assert');
const path = require('path');

const globs = {
  win32: [
    ['foo\\bar\\baz', 'foo\\[bcr]ar\\baz', true], // Matches 'bar' or 'car' in 'foo\\bar'
    ['foo\\bar\\baz', 'foo\\[!bcr]ar\\baz', false], // Matches anything except 'bar' or 'car' in 'foo\\bar'
    ['foo\\bar\\baz', 'foo\\[bc-r]ar\\baz', true], // Matches 'bar' or 'car' using range in 'foo\\bar'
    ['foo\\bar\\baz', 'foo\\*\\!bar\\*\\baz', false], // Matches anything with 'foo' and 'baz' but not 'bar' in between
    ['foo\\bar1\\baz', 'foo\\bar[0-9]\\baz', true], // Matches 'bar' followed by any digit in 'foo\\bar1'
    ['foo\\bar5\\baz', 'foo\\bar[0-9]\\baz', true], // Matches 'bar' followed by any digit in 'foo\\bar5'
    ['foo\\barx\\baz', 'foo\\bar[a-z]\\baz', true], // Matches 'bar' followed by any lowercase letter in 'foo\\barx'
    ['foo\\bar\\baz\\boo', 'foo\\[bc-r]ar\\baz\\*', true], // Matches 'bar' or 'car' in 'foo\\bar'
    ['foo\\bar\\baz', 'foo/**', true], // Matches anything in 'foo'
    ['foo\\bar\\baz', '*', false], // No match
  ],
  posix: [
    ['foo/bar/baz', 'foo/[bcr]ar/baz', true], // Matches 'bar' or 'car' in 'foo/bar'
    ['foo/bar/baz', 'foo/[!bcr]ar/baz', false], // Matches anything except 'bar' or 'car' in 'foo/bar'
    ['foo/bar/baz', 'foo/[bc-r]ar/baz', true], // Matches 'bar' or 'car' using range in 'foo/bar'
    ['foo/bar/baz', 'foo/*/!bar/*/baz', false], // Matches anything with 'foo' and 'baz' but not 'bar' in between
    ['foo/bar1/baz', 'foo/bar[0-9]/baz', true], // Matches 'bar' followed by any digit in 'foo/bar1'
    ['foo/bar5/baz', 'foo/bar[0-9]/baz', true], // Matches 'bar' followed by any digit in 'foo/bar5'
    ['foo/barx/baz', 'foo/bar[a-z]/baz', true], // Matches 'bar' followed by any lowercase letter in 'foo/barx'
    ['foo/bar/baz/boo', 'foo/[bc-r]ar/baz/*', true], // Matches 'bar' or 'car' in 'foo/bar'
    ['foo/bar/baz', 'foo/**', true], // Matches anything in 'foo'
    ['foo/bar/baz', '*', false], // No match
  ],
};


for (const [platform, platformGlobs] of Object.entries(globs)) {
  for (const [pathStr, glob, expected] of platformGlobs) {
    const actual = path[platform].matchesGlob(pathStr, glob);
    assert.strictEqual(actual, expected, `Expected ${pathStr} to ` + (expected ? '' : 'not ') + `match ${glob} on ${platform}`);
  }
}

// Test for non-string input
assert.throws(() => path.matchesGlob(123, 'foo/bar/baz'), /.*must be of type string.*/);
assert.throws(() => path.matchesGlob('foo/bar/baz', 123), /.*must be of type string.*/);

// Test exclude functionality
const excludeTests = {
  win32: [
    // Basic exclude with string
    ['foo\\bar\\baz', 'foo\\**\\*', { exclude: 'foo\\bar\\*' }, false],
    ['foo\\bar\\baz', 'foo\\**\\*', { exclude: 'foo\\other\\*' }, true],

    // Exclude with array
    ['foo\\bar\\baz', 'foo\\**\\*', { exclude: ['foo\\bar\\*', 'foo\\other\\*'] }, false],
    ['foo\\test\\file', 'foo\\**\\*', { exclude: ['foo\\bar\\*', 'foo\\other\\*'] }, true],

    // Exclude with function
    ['foo\\bar\\baz', 'foo\\**\\*', { exclude: (path) => path.includes('bar') }, false],
    ['foo\\test\\file', 'foo\\**\\*', { exclude: (path) => path.includes('bar') }, true],
  ],
  posix: [
    // Basic exclude with string
    ['foo/bar/baz', 'foo/**/*', { exclude: 'foo/bar/*' }, false],
    ['foo/bar/baz', 'foo/**/*', { exclude: 'foo/other/*' }, true],

    // Exclude with array
    ['foo/bar/baz', 'foo/**/*', { exclude: ['foo/bar/*', 'foo/other/*'] }, false],
    ['foo/test/file', 'foo/**/*', { exclude: ['foo/bar/*', 'foo/other/*'] }, true],

    // Exclude with function
    ['foo/bar/baz', 'foo/**/*', { exclude: (path) => path.includes('bar') }, false],
    ['foo/test/file', 'foo/**/*', { exclude: (path) => path.includes('bar') }, true],
  ],
};

for (const [platform, platformTests] of Object.entries(excludeTests)) {
  for (const [pathStr, pattern, options, expected] of platformTests) {
    const actual = path[platform].matchesGlob(pathStr, pattern, options);
    assert.strictEqual(actual, expected,
                       `Expected ${pathStr} to ${expected ? '' : 'not '}match ${pattern} with exclude on ${platform}`);
  }
}
