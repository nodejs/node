'use strict';

require('../common');
const path = require('path');
const fs = require('fs');
const assert = require('assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const tmpDir = tmpdir.path;

const node_modules = path.join(tmpDir, 'node_modules');

fs.mkdirSync(node_modules);
fs.mkdirSync(path.join(node_modules, 'util'));
fs.writeFileSync(path.join(node_modules, 'util', 'xyz.js'),
                 'throw new Error("this should not run")');

fs.writeFileSync(path.join(tmpDir, 'test.js'),
                 'module.exports = () => require("util/xyz");');

fs.writeFileSync(path.join(tmpDir, 'test.mjs'),
                 'export default () => import("util/xyz.js");');

{
  const test = require(path.join(tmpDir, 'test.js'));
  assert.throws(() => {
    test();
  }, {
    code: 'MODULE_NOT_FOUND',
  });
}

(async function testESM() {
  const { default: test } = await import(path.join(tmpDir, 'test.mjs'));
  assert.rejects(() => test(), {
    code: 'ERR_UNKNOWN_BUILTIN_MODULE',
  });
}());
