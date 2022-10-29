// Flags: --expose-internals
'use strict';

require('../common');
const { strictEqual } = require('assert');

// Ensures that semver is bundled & basic functionality works
const semver = require('internal/deps/npm/node_modules/semver/semver');
strictEqual(semver.gt('2.0.0', '1.0.0'), true);
strictEqual(semver.gt('1.0.0', '2.0.0'), false);

strictEqual(semver.lt('2.0.0', '1.0.0'), false);
strictEqual(semver.lt('1.0.0', '2.0.0'), true);
