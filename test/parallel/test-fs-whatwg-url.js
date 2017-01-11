'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const os = require('os');
const URL = require('url').URL;
const Buffer = require('buffer').Buffer;

function pathToFileURL(p) {
  if (!path.isAbsolute(p))
    throw new Error('Path must be absolute');
  if (common.isWindows && p.startsWith('\\\\'))
    p = p.slice(2);
  return new URL(`file://${p}`);
}

const p = path.resolve(common.fixturesDir, 'a.js');
const url = pathToFileURL(p);

assert(url instanceof URL);

// Check that we can pass in a URL object successfully
fs.readFile(url, common.mustCall((err, data) => {
  assert.ifError(err);
  assert(Buffer.isBuffer(data));
}));

// Check that using a non file:// URL reports an error
const httpUrl = new URL('http://example.org');
fs.readFile(httpUrl, common.mustCall((err) => {
  assert(err);
  assert.strictEqual(err.message,
                     'Only `file:` URLs are supported');
}));

// pct-encoded characters in the path will be decoded and checked
fs.readFile(new URL('file:///c:/tmp/%00test'), common.mustCall((err) => {
  assert(err);
  assert.strictEqual(err.message,
                     'Path must be a string without null bytes');
}));

if (common.isWindows) {
  // encoded back and forward slashes are not permitted on windows
  ['%2f', '%2F', '%5c', '%5C'].forEach((i) => {
    fs.readFile(new URL(`file:///c:/tmp/${i}`), common.mustCall((err) => {
      assert(err);
      assert.strictEqual(err.message,
                         'Path must not include encoded \\ or / characters');
    }));
  });
} else {
  // encoded forward slashes are not permitted on other platforms
  ['%2f', '%2F'].forEach((i) => {
    fs.readFile(new URL(`file:///c:/tmp/${i}`), common.mustCall((err) => {
      assert(err);
      assert.strictEqual(err.message,
                         'Path must not include encoded / characters');
    }));
  });

  fs.readFile(new URL('file://hostname/a/b/c'), common.mustCall((err) => {
    assert(err);
    assert.strictEqual(err.message,
                       `File URLs on ${os.platform()} must use ` +
                       'hostname \'localhost\' or not specify any hostname');
  }));
}
