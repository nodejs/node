// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const { versionCheck } = require('internal/v8_prof_polyfill');

assert.strictEqual(versionCheck('v8-version,1,2,3,4,0', '1.2.3'), undefined);
assert.strictEqual(versionCheck('v8-version,1,2,3,4,0', '1.2.3.3'), undefined);
assert.strictEqual(versionCheck('v8-version,1,2,3,4,0', '1.2.3.4'), undefined);
assert.strictEqual(versionCheck('v8-version,1,2,3,4,0', '1.2.3.5'), undefined);
assert.strictEqual(versionCheck('v8-version,1,2,3,4,-node.1,0', '1.2.3'),
                   undefined);
assert.strictEqual(versionCheck('v8-version,1,2,3,4,-node.1,0', '1.2.3.4'),
                   undefined);
assert.strictEqual(versionCheck('v8-version,1,2,3,4,-node.1,0', '1.2.3-node.1'),
                   undefined);
assert.strictEqual(versionCheck('v8-version,1,2,3,4,-node.1,0', '1.2.3-node.2'),
                   undefined);
assert.strictEqual(
  versionCheck('v8-version,1,2,3,4,-node.1,0', '1.2.3.4-node.2'),
  undefined);

{
  const expected = 'Unable to read v8-version from log file.';
  assert.strictEqual(versionCheck('faux', '1.2.3'), expected);
  assert.strictEqual(versionCheck('v8-version', '1.2.3'), expected);
  assert.strictEqual(versionCheck('v8-version,1', '1.2.3'), expected);
  assert.strictEqual(versionCheck('v8-version,1,2', '1.2.3'), expected);
  assert.strictEqual(versionCheck('v8-version,1,2,3', '1.2.3'), expected);
  assert.strictEqual(versionCheck('v8-version,1,2,3,4', '1.2.3'), expected);
  assert.strictEqual(versionCheck('v8-version,1,2,3,4,5,6,7', '1.2.3'),
                     expected);
}

{
  const expected = 'Testing v8 version different from logging version';
  assert.strictEqual(versionCheck('v8-version,4,3,2,1,0', '1.2.3'), expected);
  assert.strictEqual(versionCheck('v8-version,4,3,2,1,0', '1.2.3.4'), expected);
  assert.strictEqual(versionCheck('v8-version,4,3,2,1,0', '4.3.1.1'), expected);
  assert.strictEqual(versionCheck('v8-version,4,3,2,1,0', '4.3.3.1'), expected);
}
