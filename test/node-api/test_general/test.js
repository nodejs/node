'use strict';

const common = require('../../common');
const test_general = require(`./build/${common.buildType}/test_general`);
const assert = require('assert');

const [ major, minor, patch, release ] = test_general.testGetNodeVersion();
assert.strictEqual(process.version.split('-')[0],
                   `v${major}.${minor}.${patch}`);
assert.strictEqual(release, process.release.name);
