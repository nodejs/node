'use strict';
const common = require('../common');
const assert = require('node:assert');
const fs = require('node:fs');

function verifyStatFsObject(statfs, isBigint = false) {
  const valueType = isBigint ? 'bigint' : 'number';

  [
    'type', 'bsize', 'blocks', 'bfree', 'bavail', 'files', 'ffree',
  ].forEach((k) => {
    assert.ok(Object.hasOwn(statfs, k));
    assert.strictEqual(typeof statfs[k], valueType,
                       `${k} should be a ${valueType}`);
  });
}

fs.statfs(__filename, common.mustSucceed(function(stats) {
  verifyStatFsObject(stats);
  assert.strictEqual(this, undefined);
}));

fs.statfs(__filename, { bigint: true }, function(err, stats) {
  assert.ifError(err);
  verifyStatFsObject(stats, true);
  assert.strictEqual(this, undefined);
});

// Synchronous
{
  const statFsObj = fs.statfsSync(__filename);
  verifyStatFsObject(statFsObj);
}

// Synchronous Bigint
{
  const statFsBigIntObj = fs.statfsSync(__filename, { bigint: true });
  verifyStatFsObject(statFsBigIntObj, true);
}

[false, 1, {}, [], null, undefined].forEach((input) => {
  assert.throws(
    () => fs.statfs(input, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => fs.statfsSync(input),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
});

// Should not throw an error
fs.statfs(__filename, undefined, common.mustCall());
