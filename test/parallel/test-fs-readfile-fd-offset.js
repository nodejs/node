'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const fdr = fs.openSync(__filename, 'r');
const fda = fs.openSync(__filename, 'a+');
const fileLengthR = fs.readFileSync(fdr).length;
const fileLengthA = fs.readFileSync(fda).length;

// reading again should result in the same length
assert.strictEqual(fileLengthR, fs.readFileSync(fdr).length);
assert.strictEqual(fileLengthA, fs.readFileSync(fda).length);

fs.readFile(fdr, common.mustCall((err, buf) => {
  if (err) throw err;
  assert.strictEqual(fileLengthR, buf.length);
}));

fs.readFile(fda, common.mustCall((err, buf) => {
  if (err) throw err;
  assert.strictEqual(fileLengthA, buf.length);
}));
