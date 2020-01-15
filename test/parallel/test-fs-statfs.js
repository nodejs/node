'use strict';
const common = require('../common');

const assert = require('assert');
const fs = require('fs');

function verifyStatFsObject(statfs, isBigint = false) {
  const valueType = isBigint ? 'bigint' : 'number';

  [
    'type', 'bsize', 'blocks', 'bfree', 'bavail', 'files', 'ffree'
  ].forEach((k) => {
    assert.ok(statfs.hasOwnProperty(k));
    assert.strictEqual(typeof statfs[k], valueType,
                       `${k} should be a ${valueType}`);
  });
}

fs.statfs(__filename, common.mustCall(function(err, stats) {
  assert.ifError(err);
  verifyStatFsObject(stats);
  // Confirm that we are not running in the context of the internal binding
  // layer.
  // Ref: https://github.com/nodejs/node/commit/463d6bac8b349acc462d345a6e298a76f7d06fb1
  assert.strictEqual(this, undefined);
}));

fs.statfs(__filename, { bigint: true }, function(err, stats) {
  assert.ifError(err);
  verifyStatFsObject(stats, true);
  // Confirm that we are not running in the context of the internal binding
  // layer.
  // Ref: https://github.com/nodejs/node/commit/463d6bac8b349acc462d345a6e298a76f7d06fb1
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
fs.statfs(__filename, undefined, common.mustCall(() => {}));
