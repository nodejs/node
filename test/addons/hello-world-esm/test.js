'use strict';
const common = require('../../common');

const assert = require('assert');
const { spawnSync } = require('child_process');
const { copyFileSync } = require('fs');
const { join } = require('path');

const buildDir = join(__dirname, 'build');

copyFileSync(join(buildDir, common.buildType, 'binding.node'),
             join(buildDir, 'binding.node'));

const result = spawnSync(process.execPath,
                         ['--experimental-modules', `${__dirname}/test.mjs`]);

assert.ifError(result.error);
// TODO: Uncomment this once ESM is no longer experimental.
// assert.strictEqual(result.stderr.toString().trim(), '');
assert.strictEqual(result.stdout.toString().trim(), 'binding.hello() = world');
