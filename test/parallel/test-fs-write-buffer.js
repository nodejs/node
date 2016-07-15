'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const Buffer = require('buffer').Buffer;
const fs = require('fs');
const filename = path.join(common.tmpDir, 'write.txt');
const expected = Buffer.from('hello');

common.refreshTmpDir();

fs.open(filename, 'w', 0o644, common.mustCall(function(err, fd) {
  if (err) throw err;

  fs.write(fd,
           expected,
           0,
           expected.length,
           null,
           common.mustCall(function(err, written) {
             if (err) throw err;

             assert.equal(expected.length, written);
             fs.closeSync(fd);

             var found = fs.readFileSync(filename, 'utf8');
             assert.deepStrictEqual(expected.toString(), found);
             fs.unlinkSync(filename);
           }));
}));
