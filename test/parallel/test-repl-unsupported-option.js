'use strict';

require('../common');

const assert = require('assert');
const { spawnSync } = require('child_process');

const result = spawnSync(process.execPath, ['-i', '--input-type=module']);

assert.strictEqual(result.stderr.toString(), 'Cannot specify --input-type for REPL\n');
assert.notStrictEqual(result.exitCode, 0);
