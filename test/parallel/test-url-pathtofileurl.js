'use strict';
const { isWindows } = require('../common');
const assert = require('assert');
const url = require('url');

{
  const fileURL = url.pathToFileURL('test/').href;
  assert.ok(fileURL.startsWith('file:///'));
  assert.ok(fileURL.endsWith('/'));
}

{
  const fileURL = url.pathToFileURL('test\\').href;
  assert.ok(fileURL.startsWith('file:///'));
  if (isWindows)
    assert.ok(fileURL.endsWith('/'));
  else
    assert.ok(fileURL.endsWith('%5C'));
}

{
  const fileURL = url.pathToFileURL('test/%').href;
  assert.ok(fileURL.includes('%25'));
}

{
  let testCases;
  if (isWindows) {
    testCases = [
      // lowercase ascii alpha
      { path: 'C:\\foo', expected: 'file:///C:/foo' },
      // uppercase ascii alpha
      { path: 'C:\\FOO', expected: 'file:///C:/FOO' },
      // dir
      { path: 'C:\\dir\\foo', expected: 'file:///C:/dir/foo' },
      // trailing separator
      { path: 'C:\\dir\\', expected: 'file:///C:/dir/' },
      // dot
      { path: 'C:\\foo.mjs', expected: 'file:///C:/foo.mjs' },
      // space
      { path: 'C:\\foo bar', expected: 'file:///C:/foo%20bar' },
      // question mark
      { path: 'C:\\foo?bar', expected: 'file:///C:/foo%3Fbar' },
      // number sign
      { path: 'C:\\foo#bar', expected: 'file:///C:/foo%23bar' },
      // ampersand
      { path: 'C:\\foo&bar', expected: 'file:///C:/foo&bar' },
      // equals
      { path: 'C:\\foo=bar', expected: 'file:///C:/foo=bar' },
      // colon
      { path: 'C:\\foo:bar', expected: 'file:///C:/foo:bar' },
      // semicolon
      { path: 'C:\\foo;bar', expected: 'file:///C:/foo;bar' },
      // percent
      { path: 'C:\\foo%bar', expected: 'file:///C:/foo%25bar' },
      // backslash
      { path: 'C:\\foo\\bar', expected: 'file:///C:/foo/bar' },
      // backspace
      { path: 'C:\\foo\bbar', expected: 'file:///C:/foo%08bar' },
      // tab
      { path: 'C:\\foo\tbar', expected: 'file:///C:/foo%09bar' },
      // newline
      { path: 'C:\\foo\nbar', expected: 'file:///C:/foo%0Abar' },
      // carriage return
      { path: 'C:\\foo\rbar', expected: 'file:///C:/foo%0Dbar' },
      // latin1
      { path: 'C:\\fóóbàr', expected: 'file:///C:/f%C3%B3%C3%B3b%C3%A0r' },
      // euro sign (BMP code point)
      { path: 'C:\\€', expected: 'file:///C:/%E2%82%AC' },
      // rocket emoji (non-BMP code point)
      { path: 'C:\\🚀', expected: 'file:///C:/%F0%9F%9A%80' }
    ];
  } else {
    testCases = [
      // lowercase ascii alpha
      { path: '/foo', expected: 'file:///foo' },
      // uppercase ascii alpha
      { path: '/FOO', expected: 'file:///FOO' },
      // dir
      { path: '/dir/foo', expected: 'file:///dir/foo' },
      // trailing separator
      { path: '/dir/', expected: 'file:///dir/' },
      // dot
      { path: '/foo.mjs', expected: 'file:///foo.mjs' },
      // space
      { path: '/foo bar', expected: 'file:///foo%20bar' },
      // question mark
      { path: '/foo?bar', expected: 'file:///foo%3Fbar' },
      // number sign
      { path: '/foo#bar', expected: 'file:///foo%23bar' },
      // ampersand
      { path: '/foo&bar', expected: 'file:///foo&bar' },
      // equals
      { path: '/foo=bar', expected: 'file:///foo=bar' },
      // colon
      { path: '/foo:bar', expected: 'file:///foo:bar' },
      // semicolon
      { path: '/foo;bar', expected: 'file:///foo;bar' },
      // percent
      { path: '/foo%bar', expected: 'file:///foo%25bar' },
      // backslash
      { path: '/foo\\bar', expected: 'file:///foo%5Cbar' },
      // backspace
      { path: '/foo\bbar', expected: 'file:///foo%08bar' },
      // tab
      { path: '/foo\tbar', expected: 'file:///foo%09bar' },
      // newline
      { path: '/foo\nbar', expected: 'file:///foo%0Abar' },
      // carriage return
      { path: '/foo\rbar', expected: 'file:///foo%0Dbar' },
      // latin1
      { path: '/fóóbàr', expected: 'file:///f%C3%B3%C3%B3b%C3%A0r' },
      // euro sign (BMP code point)
      { path: '/€', expected: 'file:///%E2%82%AC' },
      // rocket emoji (non-BMP code point)
      { path: '/🚀', expected: 'file:///%F0%9F%9A%80' },
    ];
  }

  for (const { path, expected } of testCases) {
    const actual = url.pathToFileURL(path).href;
    assert.strictEqual(actual, expected);
  }
}
