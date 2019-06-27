'use strict';

const common = require('../../common');
const addonPath = `./build/${common.buildType}/test_general`;
const resolvedPath = require.resolve(addonPath);
const test_general = require(addonPath);
const test_general_module = require.cache[resolvedPath];
const assert = require('assert');

const [ major, minor, patch, release ] = test_general.testGetNodeVersion();
assert.strictEqual(process.version.split('-')[0],
                   `v${major}.${minor}.${patch}`);
assert.strictEqual(release, process.release.name);

assert.strictEqual(test_general_module, test_general.testGetModule());
