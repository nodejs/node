'use strict';

const common = require('../../common');
const filename = require.resolve(`./build/${common.buildType}/test_general`);
const test_general = require(filename);
const assert = require('assert');

assert.strictEqual(test_general.filename, filename);

const [ major, minor, patch, release ] = test_general.testGetNodeVersion();
assert.strictEqual(process.version.split('-')[0],
                   `v${major}.${minor}.${patch}`);
assert.strictEqual(release, process.release.name);
