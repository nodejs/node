'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');
const expected = 'xyz\n';

fs.read(fd,
        expected.length,
        0,
        'utf-8',
        common.mustCall((err, str, bytesRead) => {
          assert.ifError(err);
          assert.strictEqual(str, expected);
          assert.strictEqual(bytesRead, expected.length);
        }));

const r = fs.readSync(fd, expected.length, 0, 'utf-8');
assert.strictEqual(r[0], expected);
assert.strictEqual(r[1], expected.length);
