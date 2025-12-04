import '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';

import assert from 'assert';
import { spawnSync } from 'child_process';
import fs from 'fs';
import { fileURLToPath } from 'url';
import util from 'util';

const debuglog = util.debuglog('test');
const versionsTool = fileURLToPath(
  new URL('../../tools/doc/versions.mjs', import.meta.url));

// At the time of writing these are the minimum expected versions.
// New versions of Node.js do not have to be explicitly added here.
const expected = [
  '12.x',
  '11.x',
  '10.x',
  '9.x',
  '8.x',
  '7.x',
  '6.x',
  '5.x',
  '4.x',
  '0.12.x',
  '0.10.x',
];

tmpdir.refresh();
const versionsFile = tmpdir.resolve('versions.json');
debuglog(`${process.execPath} ${versionsTool} ${versionsFile}`);
const opts = { cwd: tmpdir.path, encoding: 'utf8' };
const cp = spawnSync(process.execPath, [ versionsTool, versionsFile ], opts);
debuglog(cp.stderr);
debuglog(cp.stdout);
assert.strictEqual(cp.stdout, '');
assert.strictEqual(cp.signal, null);
assert.strictEqual(cp.status, 0);
const versions = JSON.parse(fs.readFileSync(versionsFile));
debuglog(versions);

// Coherence checks for each returned version.
for (const version of versions) {
  const tested = util.inspect(version);
  const parts = version.num.split('.');
  const expectedLength = parts[0] === '0' ? 3 : 2;
  assert.strictEqual(parts.length, expectedLength,
                     `'num' from ${tested} should be '<major>.x'.`);
  assert.strictEqual(parts[parts.length - 1], 'x',
                     `'num' from ${tested} doesn't end in '.x'.`);
  const isEvenRelease = Number.parseInt(parts[expectedLength - 2]) % 2 === 0;
  const hasLtsProperty = Object.hasOwn(version, 'lts');
  if (hasLtsProperty) {
    // Odd-numbered versions of Node.js are never LTS.
    assert.ok(isEvenRelease, `${tested} should not be an 'lts' release.`);
    assert.ok(version.lts, `'lts' from ${tested} should 'true'.`);
  }
}

// Check that the minimum number of versions were returned.
// Later versions are allowed, but not checked for here (they were checked
// above).
// Also check for the previous semver major -- From the main branch this will be
// the most recent major release.
const thisMajor = Number.parseInt(process.versions.node.split('.')[0]);
const prevMajorString = `${thisMajor - 1}.x`;
if (!expected.includes(prevMajorString)) {
  expected.unshift(prevMajorString);
}
for (const version of expected) {
  assert.ok(versions.find((x) => x.num === version),
            `Did not find entry for '${version}' in ${util.inspect(versions)}`);
}
