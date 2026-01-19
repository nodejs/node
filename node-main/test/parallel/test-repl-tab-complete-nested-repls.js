// Tab completion sometimes uses a separate REPL instance under the hood.
// That REPL instance has its own domain. Make sure domain errors trickle back
// up to the main REPL.
//
// Ref: https://github.com/nodejs/node/issues/21586

'use strict';

require('../common');
const fixtures = require('../common/fixtures');

const assert = require('assert');
const { spawnSync } = require('child_process');

const testFile = fixtures.path('repl-tab-completion-nested-repls.js');
const result = spawnSync(process.execPath, [testFile]);

// The spawned process will fail. In Node.js 10.11.0, it will fail silently. The
// test here is to make sure that the error information bubbles up to the
// calling process.
assert.ok(result.status, 'testFile swallowed its error');
const err = result.stderr.toString();
assert.ok(err.includes('fhqwhgads'), err);
