'use strict';

const common = require('../common');

// This test does not work on OSX due to the way it handles
// non-Unicode sequences in file names.
if (common.isMacOS) {
  common.skip('Test unsupported on OSX');
}

// Unfortunately, the test also does not work on Windows
// because the writeFileSync operation will replace the
// non-Unicode characters with replacement characters when
// it normalizes the path.
if (common.isWindows) {
  common.skip('Test unsupported on Windows');
}

const tmpdir = require('../common/tmpdir');

const {
  existsSync,
  writeFileSync,
} = require('node:fs');

const {
  ok,
  throws,
} = require('node:assert');

const {
  sep,
} = require('node:path');

tmpdir.refresh();

const {
  pathToFileURL,
  fileURLToPath,
  fileURLToPathBuffer,
} = require('node:url');

const kShiftJisName = '%82%A0%82%A2%82%A4';
const kShiftJisBuffer = Buffer.from([0x82, 0xA0, 0x82, 0xA2, 0x82, 0xA4]);

const tmpdirUrl = pathToFileURL(tmpdir.path + sep);
const testPath = new URL(kShiftJisName, tmpdirUrl);

ok(testPath.pathname.endsWith(`/${kShiftJisName}`));

const tmpdirBuffer = Buffer.from(tmpdir.path + sep, 'utf8');
const testPathBuffer = Buffer.concat([tmpdirBuffer, kShiftJisBuffer]);

// We can use the Buffer version of the path to create a file and check
// its existence. But we cannot use the URL version because it contains
// non-Unicode percent-encoded characters.
throws(() => writeFileSync(testPath, 'test'), {
  name: 'URIError',
});

writeFileSync(testPathBuffer, 'test');
ok(existsSync(testPathBuffer));

// Using fileURLToPath fails because the URL contains non-Unicode
// percent-encoded characters.
throws(() => existsSync(fileURLToPath(testPath)), {
  name: 'URIError',
});

// This variation succeeds because the URL is converted to a buffer
// without trying to interpret the percent-encoded characters.
ok(existsSync(fileURLToPathBuffer(testPath)));
