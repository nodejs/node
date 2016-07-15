'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const filepath = path.join(common.fixturesDir, 'x.txt');
const fd = fs.openSync(filepath, 'r');
const expected = 'xyz\n';

fs.read(fd,
        expected.length,
        0,
        'utf-8',
        common.mustCall(function(err, str, bytesRead) {
          assert.ok(!err);
          assert.equal(str, expected);
          assert.equal(bytesRead, expected.length);
        }));

var r = fs.readSync(fd, expected.length, 0, 'utf-8');
assert.equal(r[0], expected);
assert.equal(r[1], expected.length);
