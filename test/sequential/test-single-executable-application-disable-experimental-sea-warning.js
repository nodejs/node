'use strict';
const common = require('../common');

// This tests the creation of a single executable application.

const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { copyFileSync, writeFileSync, existsSync } = require('fs');
const { execFileSync } = require('child_process');
const { join } = require('path');
const { strictEqual } = require('assert');
const assert = require('assert');

common.skipIfSingleExecutableIsNotSupported();

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
  "disableExperimentalSEAWarning": true
}
`);

// Copy input to working directory
copyFileSync(inputFile, join(tmpdir.path, 'sea.js'));
execFileSync(process.execPath, ['--experimental-sea-config', 'sea-config.json'], {
  cwd: tmpdir.path
});

assert(existsSync(seaPrepBlob));

copyFileSync(process.execPath, outputFile);
const postjectFile = fixtures.path('postject-copy', 'node_modules', 'postject', 'dist', 'cli.js');
execFileSync(process.execPath, [
  postjectFile,
  outputFile,
  'NODE_SEA_BLOB',
  seaPrepBlob,
  '--sentinel-fuse', 'NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2',
  ...process.platform === 'darwin' ? [ '--macho-segment-name', 'NODE_SEA' ] : [],
]);

if (process.platform === 'darwin') {
  execFileSync('codesign', [ '--sign', '-', outputFile ]);
  execFileSync('codesign', [ '--verify', outputFile ]);
} else if (process.platform === 'win32') {
  let signtoolFound = false;
  try {
    execFileSync('where', [ 'signtool' ]);
    signtoolFound = true;
  } catch (err) {
    console.log(err.message);
  }
  if (signtoolFound) {
    let certificatesFound = false;
    try {
      execFileSync('signtool', [ 'sign', '/fd', 'SHA256', outputFile ]);
      certificatesFound = true;
    } catch (err) {
      if (!/SignTool Error: No certificates were found that met all the given criteria/.test(err)) {
        throw err;
      }
    }
    if (certificatesFound) {
      execFileSync('signtool', 'verify', '/pa', 'SHA256', outputFile);
    }
  }
}

const singleExecutableApplicationOutput = execFileSync(
  outputFile,
  [ '-a', '--b=c', 'd' ],
  { env: { COMMON_DIRECTORY: join(__dirname, '..', 'common') } });
strictEqual(singleExecutableApplicationOutput.toString(), 'Hello, world! 😊\n');
