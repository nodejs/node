'use strict';

// This tests heap snapshot integration of a pending recursive mkdir.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const { validateByRetainingPath } = require('../common/heap');
const assert = require('assert');
const fs = require('fs');

tmpdir.refresh();

{
  const nodes = validateByRetainingPath('Node / FSContinuationData', []);
  assert.strictEqual(nodes.length, 0);
}

fs.mkdir(tmpdir.resolve('a', 'b'), { recursive: true }, common.mustSucceed());

{
  const nodes = validateByRetainingPath('Node / FSContinuationData', []);
  assert.strictEqual(nodes.length, 1);
}
