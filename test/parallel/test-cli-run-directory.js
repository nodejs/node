'use strict';
require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { spawnSync } = require('child_process');
const { sep } = require('path');

const outerWd = fixtures.path('cli-current-dir-over-parent');
const innerWd = fixtures.path('cli-current-dir-over-parent', 'inner');

// previously node would run ../inner.js in this case instead of ./index.js
let result = spawnSync(process.execPath, ['.'], { cwd: innerWd });
assert.strictEqual(result.status, 0);
assert.strictEqual(result.output[1] + '', 'inner/index.js\n');

result = spawnSync(process.execPath, ['.' + sep], { cwd: innerWd });
assert.strictEqual(result.status, 0);
assert.strictEqual(result.output[1] + '', 'inner/index.js\n');

// ensure works when reaching up
result = spawnSync(process.execPath, ['..' + sep + 'inner'], { cwd: innerWd });
assert.strictEqual(result.status, 0);
assert.strictEqual(result.output[1] + '', 'inner.js\n');

// ensure works when reaching up
result = spawnSync(process.execPath, ['..' + sep + 'inner'], { cwd: innerWd });
assert.strictEqual(result.status, 0);
assert.strictEqual(result.output[1] + '', 'inner.js\n');

// can have slash and dot
result = spawnSync(process.execPath, ['.' + sep + '.'], { cwd: innerWd });
assert.strictEqual(result.status, 0);
assert.strictEqual(result.output[1] + '', 'inner/index.js\n');

// should error because ../ has no index.
result = spawnSync(process.execPath, ['..' + sep], { cwd: innerWd });
assert.strictEqual(result.status, 1);

// gets inner.js when outside
result = spawnSync(process.execPath, ['.' + sep + 'inner'], { cwd: outerWd });
assert.strictEqual(result.status, 0);
assert.strictEqual(result.output[1] + '', 'inner.js\n');

// gets index when outside and trailing slash
result = spawnSync(process.execPath, [
  '.' + sep + 'inner' + sep
], { cwd: outerWd });
assert.strictEqual(result.status, 0);
assert.strictEqual(result.output[1] + '', 'inner/index.js\n');
