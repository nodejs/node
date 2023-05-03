'use strict';

require('../common');

const {
  injectAndCodeSign,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

// This tests the creation of a single executable application.

const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { copyFileSync, writeFileSync, existsSync } = require('fs');
const { execFileSync } = require('child_process');
const { join } = require('path');
const { strictEqual } = require('assert');
const assert = require('assert');

const inputFile = fixtures.path('sea.js');
const requirableFile = join(tmpdir.path, 'requirable.js');
const configFile = join(tmpdir.path, 'sea-config.json');
const seaPrepBlob = join(tmpdir.path, 'sea-prep.blob');
const outputFile = join(tmpdir.path, process.platform === 'win32' ? 'sea.exe' : 'sea');

tmpdir.refresh();

writeFileSync(requirableFile, `
module.exports = {
  hello: 'world',
};
`);

writeFileSync(configFile, `
{
  "main": "sea.js",
  "output": "sea-prep.blob",
  "disableExperimentalSEAWarning": false
}
`);

// Copy input to working directory
copyFileSync(inputFile, join(tmpdir.path, 'sea.js'));
execFileSync(process.execPath, ['--experimental-sea-config', 'sea-config.json'], {
  cwd: tmpdir.path
});

assert(existsSync(seaPrepBlob));

copyFileSync(process.execPath, outputFile);
injectAndCodeSign(outputFile, seaPrepBlob);

const singleExecutableApplicationOutput = execFileSync(
  outputFile,
  [ '-a', '--b=c', 'd' ],
  { env: { COMMON_DIRECTORY: join(__dirname, '..', 'common') } });
strictEqual(singleExecutableApplicationOutput.toString(), 'Hello, world! 😊\n');
