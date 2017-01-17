'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const Buffer = require('buffer').Buffer;
const fs = require('fs');
const filepath = path.join(common.fixturesDir, 'x.txt');
const fd = fs.openSync(filepath, 'r');

const expected = Buffer.from('xyz\n');

function test(bufferAsync, bufferSync, expected) {
  fs.read(fd,
          bufferAsync,
          0,
          expected.length,
          0,
          common.mustCall((err, bytesRead) => {
            assert.ifError(err);
            assert.strictEqual(bytesRead, expected.length);
            assert.deepStrictEqual(bufferAsync, Buffer.from(expected));
          }));

  const r = fs.readSync(fd, bufferSync, 0, expected.length, 0);
  assert.deepStrictEqual(bufferSync, Buffer.from(expected));
  assert.strictEqual(r, expected.length);
}

test(Buffer.allocUnsafe(expected.length),
     Buffer.allocUnsafe(expected.length),
     expected);

test(new Uint8Array(expected.length),
     new Uint8Array(expected.length),
     Uint8Array.from(expected));
