'use strict';

const common = require('../common');

const assert = require('assert');
const versionParts = process.versions.node.split('.');

assert.strictEqual(process.release.name, 'node');

// it's expected that future LTS release lines will have additional
// branches in here
if (versionParts[0] === '4' && versionParts[1] >= 2) {
  assert.strictEqual(process.release.lts, 'Argon');
} else if (versionParts[0] === '6' && versionParts[1] >= 9) {
  assert.strictEqual(process.release.lts, 'Boron');
} else if (versionParts[0] === '8' && versionParts[1] >= 9) {
  assert.strictEqual(process.release.lts, 'Carbon');
} else {
  assert.strictEqual(process.release.lts, undefined);
}

const {
  majorVersion: major,
  minorVersion: minor,
  patchVersion: patch,
  prereleaseTag: tag,
  compareVersion,
} = process.release;

assert.strictEqual(compareVersion(major, minor, patch, tag), 0);

assert.strictEqual(compareVersion(major + 1, minor, patch, tag), -1);
assert.strictEqual(compareVersion(major - 1, minor, patch, tag), 1);

assert.strictEqual(compareVersion(major, minor + 1, patch, tag), -1);
assert.strictEqual(compareVersion(major, minor - 1, patch, tag), 1);

assert.strictEqual(compareVersion(major, minor, patch + 1, tag), -1);
assert.strictEqual(compareVersion(major, minor, patch - 1, tag), 1);

if (tag)
  assert.strictEqual(compareVersion(major, minor, patch), 1);
else
  assert.strictEqual(compareVersion(major, minor, patch, 'notrealtag'), -1);

for (const args of [
  ['', 0, 0, ''],
  [0, '', 0, ''],
  [0, 0, '', ''],
  [0, 0, 0, 0],
]) {
  common.expectsError(() => {
    compareVersion(args[0], args[1], args[2], args[3]);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}
