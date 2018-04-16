'use strict';

require('../common');

const assert = require('assert');
const versionParts = process.versions.node.split('.');

assert.strictEqual(process.release.name, 'node');

// It's expected that future LTS release lines will have additional
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

[
  [major, minor, patch, tag, 0],
  [major + 1, minor, patch, tag, -1],
  [major - 1, minor, patch, tag, 1],
  [major, minor + 1, patch, tag, -1],
  [major, minor - 1, patch, tag, 1],
  [major, minor, patch + 1, tag, -1],
  [major, minor, patch - 1, tag, 1]
].forEach(([major, minor, patch, tag, expected]) => {
  // Skip invalid entries.
  if (major < 0 || minor < 0 || patch < 0)
    return;
  assert.strictEqual(compareVersion(major, minor, patch, tag), expected);
  const semverStr = `${major}.${minor}.${patch}${tag ? `-${tag}` : ''}`;
  assert.strictEqual(compareVersion(semverStr), expected);
});

if (tag) {
  assert.strictEqual(compareVersion(major, minor, patch), 1);
  assert.strictEqual(compareVersion(`${major}.${minor}.${patch}`), 1);
  assert.strictEqual(compareVersion(major, minor, patch, `${tag}-fake`), -1);
  assert.strictEqual(
    compareVersion(major, minor, patch, `${tag.slice(0, -1)}`), 1);
} else {
  assert.strictEqual(compareVersion(major, minor, patch, 'fake-tag'), -1);
  assert.strictEqual(compareVersion(`${major}.${minor}.${patch}-fake-tag`), -1);
  assert.strictEqual(compareVersion(major, minor, patch), 0);
}

[
  [['1', 0, 0, ''], 'major', 'string'],
  [[0, '1', 0, ''], 'minor', 'string'],
  [[0, 0, '1', ''], 'patch', 'string'],
  [[0, 0, 0, 1], 'tag', 'number'],
  [[0, 0, 0, null], 'tag', 'object'],
].forEach(([args, argName, receivedType]) => {
  const expectedType = argName === 'tag' ? 'string' : 'number';
  assert.throws(() => {
    compareVersion(...args);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError [ERR_INVALID_ARG_TYPE]',
    message: `The "${argName}" argument must be of type ${expectedType}. ` +
             `Received type ${receivedType}`
  });
});

[
  [-1, 0, 0, 'major'],
  [0, -1, 0, 'minor'],
  [0, 0, -1, 'patch'],
].forEach(([major, minor, patch, expected]) => {
  assert.throws(() => {
    compareVersion(major, minor, patch);
  }, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: `The value of "${expected}" is out of range. ` +
             'It must be >= 0. Received -1'
  });
});

[
  [1.5, 1, 1, 'major'],
  [1, 1.5, 1, 'minor'],
  [1, 1, 1.5, 'patch'],
].forEach(([major, minor, patch, expected]) => {
  assert.throws(() => {
    compareVersion(major, minor, patch);
  }, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: `The value of "${expected}" is out of range. ` +
             'It must be an integer. Received 1.5'
  });
});

[
  '1a.0.0',
  '1.0a.0',
  '1.0.0a',
  '1.0.0.0',
  '01.0.0',
  '1.01.0',
  '1.0',
  '1..0',
].forEach((version) => {
  assert.throws(() => {
    compareVersion(version);
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError [ERR_INVALID_ARG_VALUE]',
    message: "The argument 'version' is not a valid Semantic Version. " +
             `Received '${version}'`
  });
});
