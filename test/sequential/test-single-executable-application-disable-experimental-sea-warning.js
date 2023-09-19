'use strict';

require('../common');

const {
  injectAndCodeSign,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

// This tests the creation of a single executable application which has the
// experimental SEA warning disabled.

const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { copyFileSync, writeFileSync, existsSync } = require('fs');
const { execFileSync } = require('child_process');
const { join } = require('path');
const { strictEqual } = require('assert');
const assert = require('assert');

const inputFile = fixtures.path('sea.js');
const requirableFile = tmpdir.resolve('requirable.js');
const configFile = tmpdir.resolve('sea-config.json');
const seaPrepBlob = tmpdir.resolve('sea-prep.blob');
const outputFile = tmpdir.resolve(process.platform === 'win32' ? 'sea.exe' : 'sea');

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
  "disableExperimentalSEAWarning": true
}
`);

// Copy input to working directory
copyFileSync(inputFile, tmpdir.resolve('sea.js'));
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
