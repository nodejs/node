'use strict';
const common = require('../common');

// This tests the creation of a single executable application.

const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { copyFileSync, readFileSync, writeFileSync } = require('fs');
const { execFileSync } = require('child_process');
const { join } = require('path');
const { strictEqual } = require('assert');

if (!process.config.variables.single_executable_application)
  common.skip('Single Executable Application support has been disabled.');

if (!['darwin', 'win32', 'linux'].includes(process.platform))
  common.skip(`Unsupported platform ${process.platform}.`);

if (process.platform === 'linux' && process.config.variables.is_debug === 1)
  common.skip('Running the resultant binary fails with `Couldn\'t read target executable"`.');

if (process.config.variables.node_shared)
  common.skip('Running the resultant binary fails with ' +
    '`/home/iojs/node-tmp/.tmp.2366/sea: error while loading shared libraries: ' +
    'libnode.so.112: cannot open shared object file: No such file or directory`.');

if (process.config.variables.icu_gyp_path === 'tools/icu/icu-system.gyp')
  common.skip('Running the resultant binary fails with ' +
    '`/home/iojs/node-tmp/.tmp.2379/sea: error while loading shared libraries: ' +
    'libicui18n.so.71: cannot open shared object file: No such file or directory`.');

if (!process.config.variables.node_use_openssl || process.config.variables.node_shared_openssl)
  common.skip('Running the resultant binary fails with `Node.js is not compiled with OpenSSL crypto support`.');

if (process.config.variables.want_separate_host_toolset !== 0)
  common.skip('Running the resultant binary fails with `Segmentation fault (core dumped)`.');

if (process.platform === 'linux') {
  const osReleaseText = readFileSync('/etc/os-release', { encoding: 'utf-8' });
  const isAlpine = /^NAME="Alpine Linux"/m.test(osReleaseText);
  if (isAlpine) common.skip('Alpine Linux is not supported.');

  if (process.arch === 's390x') {
    common.skip('On s390x, postject fails with `memory access out of bounds`.');
  }

  if (process.arch === 'ppc64') {
    common.skip('On ppc64, this test times out.');
  }
}

const inputFile = fixtures.path('sea.js');
const requirableFile = join(tmpdir.path, 'requirable.js');
const outputFile = join(tmpdir.path, process.platform === 'win32' ? 'sea.exe' : 'sea');

tmpdir.refresh();

writeFileSync(requirableFile, `
module.exports = {
  hello: 'world',
};
`);

copyFileSync(process.execPath, outputFile);
const postjectFile = fixtures.path('postject-copy', 'node_modules', 'postject', 'dist', 'cli.js');
execFileSync(process.execPath, [
  postjectFile,
  outputFile,
  'NODE_JS_CODE',
  inputFile,
  '--sentinel-fuse', 'NODE_JS_FUSE_fce680ab2cc467b6e072b8b5df1996b2',
  ...process.platform === 'darwin' ? [ '--macho-segment-name', 'NODE_JS' ] : [],
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
