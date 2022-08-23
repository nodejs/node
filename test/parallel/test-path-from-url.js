'use strict';

const common = require('../common');
const assert = require('node:assert');
const { fromURL, win32 } = require('node:path');
const { describe, it } = require('node:test');


describe('path.fromURL', { concurrency: true }, () => {
  it('should passthrough path string', () => assert
    .strictEqual(fromURL('/Users/node/dev'), '/Users/node/dev'));
  it('should passthrough an empty string', () => assert
    .strictEqual(fromURL(''), ''));
  it('should passthrough a URL string with file schema', () => assert
    .strictEqual(fromURL('file:///Users/node/dev'), 'file:///Users/node/dev'));
  it('should parse a URL instance with file schema', () => assert
    .strictEqual(fromURL(new URL('file:///Users/node/dev')), '/Users/node/dev'));
  it('should parse a URL instance with file schema', () => assert
    .strictEqual(fromURL(new URL('file:///path/to/file')), '/path/to/file'));
  it('should remove the host from a URL instance', () => assert
    .strictEqual(fromURL(new URL('file://localhost/etc/fstab')), '/etc/fstab'));
  it('should parse a windows file URI', () => {
    const fn = common.isWindows ? fromURL : win32.fromURL;
    assert.strictEqual(fn(new URL('file:///c:/foo.txt')), 'c:\\foo.txt');
  });
  it('should throw an error if the URL is not a file URL', () => assert
    .throws(() => fromURL(new URL('http://localhost/etc/fstab')), { code: 'ERR_INVALID_URL_SCHEME' }));
  it('should throw an error if converting invalid types', () => [{}, [], 1, null, undefined,
                                                                 Promise.resolve(new URL('file:///some/path/')), Symbol(), true, false, 1n, 0, 0n, () => {}]
    .forEach((item) => assert.throws(() => fromURL(item), { code: 'ERR_INVALID_ARG_TYPE' })));
});
