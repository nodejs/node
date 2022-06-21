'use strict';

require('../common');

const assert = require('assert');
const { spawnSync } = require('child_process');

const result = spawnSync(process.execPath, ['-i', '--input-type=module']);

assert.match(result.stderr.toString(), /Cannot specify --input-type for REPL/);
