'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

const results = new Set();
for (let i = 0; i < 10; i++) {
  const result = spawnSync(process.execPath, ['-p', 'Math.random()']);
  assert.strictEqual(result.status, 0);
  results.add(result.stdout.toString());
}
// It's theoretically possible if _very_ unlikely to see some duplicates.
// Therefore, don't expect that the size of the set is exactly 10 but do
// assume it's > 1 because if you get 10 duplicates in a row you should
// go out real quick and buy some lottery tickets, you lucky devil you!
assert(results.size > 1);
