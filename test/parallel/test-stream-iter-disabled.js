'use strict';
const common = require('../common');
const assert = require('assert');
const { spawnPromisified } = common;

async function testRequireNodeStreamIterWithoutFlag() {
  const { stderr, code } = await spawnPromisified(process.execPath, [
    '-e', 'require("node:stream/iter")',
  ]);
  assert.match(stderr, /No such built-in module: node:stream\/iter/);
  assert.notStrictEqual(code, 0);
}

async function testRequireStreamIterWithoutFlag() {
  const { stderr, code } = await spawnPromisified(process.execPath, [
    '-e', 'require("stream/iter")',
  ]);
  assert.match(stderr, /Cannot find module/);
  assert.notStrictEqual(code, 0);
}

async function testRequireWithFlag() {
  const { code } = await spawnPromisified(process.execPath, [
    '--experimental-stream-iter',
    '-e', 'require("node:stream/iter")',
  ]);
  assert.strictEqual(code, 0);
}

Promise.all([
  testRequireNodeStreamIterWithoutFlag(),
  testRequireStreamIterWithoutFlag(),
  testRequireWithFlag(),
]).then(common.mustCall());
