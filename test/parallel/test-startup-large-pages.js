'use strict';

// A sanity check: Make sure that Node.js runs correctly when running with the
// --use-largepages option.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

const child = spawnSync(process.execPath, [ '--use-largepages', '-p', '42' ]);
const stdout = child.stdout.toString().match(/\S+/g);

assert.strictEqual(child.status, 0);
assert.strictEqual(child.signal, null);
assert.strictEqual(stdout.length, 1);
assert.strictEqual(stdout[0], '42');

// TODO(gabrielschulhof): Make assertions about the stderr, which may or may not
// contain a message indicating that mapping to large pages has failed.
