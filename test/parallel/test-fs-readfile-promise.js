'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const emptyFile = path.join(common.fixturesDir, 'empty.txt');
const fakeFile = path.join('./fakefile');

fs.readFile(emptyFile, 'utf8').then((data) => {
  assert.strictEqual(data, '');
});

fs.readFile(fakeFile, 'utf8').catch((err) => {
  assert(err);
});
