'use strict';
const common = require('../common');

const assert = require('assert');
const fs = require('fs');

fs.statfs(__filename, common.mustCall(function(err, stats) {
  assert.ifError(err);
  [
    'type', 'bsize', 'blocks', 'bfree', 'bavail', 'files', 'ffree'
  ].forEach((k) => {
    assert.ok(stats.hasOwnProperty(k));
    assert.strictEqual(typeof stats[k], 'number', `${k} should be a number`);
  });
  assert.ok(stats.hasOwnProperty('spare'));
  assert.ok(stats.spare instanceof Array);
  // Confirm that we are not running in the context of the internal binding
  // layer.
  // Ref: https://github.com/nodejs/node/commit/463d6bac8b349acc462d345a6e298a76f7d06fb1
  assert.strictEqual(this, undefined);
}));

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
