'use strict';
const common = require('../common');
const assert = require('assert');
const versionParts = process.versions.node.split('.');

assert.equal(process.release.name, 'node');

// it's expected that future LTS release lines will have additional
// branches in here
if (versionParts[0] === '4' && versionParts[1] >= 2) {
  assert.equal(process.release.lts, 'Argon');
} else {
  assert.strictEqual(process.release.lts, undefined);
}
