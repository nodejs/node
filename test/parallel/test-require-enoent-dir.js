'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();

const fooPath = tmpdir.resolve('foo.cjs');
fs.writeFileSync(fooPath, '');

const dirPath = tmpdir.resolve('delete_me');
fs.mkdirSync(dirPath, {
  recursive: true
});

const barPath = path.join(dirPath, 'bar.cjs');
fs.writeFileSync(barPath, `
    module.exports = () => require('../foo.cjs').call()
`);

const foo = require(fooPath);
const unique = Symbol('unique');
foo.call = common.mustCall(() => unique);
const bar = require(barPath);

fs.rmSync(dirPath, { recursive: true });
assert.strict.equal(bar(), unique);
