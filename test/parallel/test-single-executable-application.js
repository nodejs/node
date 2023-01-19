'use strict';
const common = require('../common');

if (!process.config.variables.single_executable_application)
  common.skip('Single Executable Application support has been disabled.');

if (process.config.variables.asan)
  common.skip('Running the resultant binary fails with `Segmentation fault (core dumped)`.');

if (process.platform === 'aix')
  common.skip('XCOFF binary format not supported.');

if (process.platform === 'freebsd')
  common.skip('Running the resultant binary fails with `Exec format error`.');

if (process.platform === 'linux' && process.config.variables.is_debug === 1)
  common.skip('Running the resultant binary fails with `Couldn\'t read target executable"`.');

if (process.config.variables.node_shared)
  common.skip('Running the resultant binary fails with ' +
    '`/home/iojs/node-tmp/.tmp.2366/sea: error while loading shared libraries: ' +
    'libnode.so.112: cannot open shared object file: No such file or directory`.');

if (!process.config.variables.node_use_openssl || process.config.variables.node_shared_openssl)
  common.skip('Running the resultant binary fails with `Node.js is not compiled with OpenSSL crypto support`.');

if (process.env.NODE_NAME === 'test-ibm-rhel8-s390x-1')
  common.skip('Running the resultant binary fails with `memory access out of bounds`.');

if (process.env.NODE_NAME === 'test-equinix_mnx-smartos20-x64-4')
  common.skip('Injection fails with `Can\'t convert PT_NOTE.virtual_address into an offset (0x0)`.');

if (process.config.variables.want_separate_host_toolset !== 0)
  common.skip('Running the resultant binary fails with `Segmentation fault (core dumped)`.');

// This tests the creation of a single executable application.

const tmpdir = require('../common/tmpdir');
const { copyFileSync, writeFileSync } = require('fs');
const { execSync } = require('child_process');
const { join } = require('path');
const { strictEqual } = require('assert');

const inputFile = join(tmpdir.path, 'sea.js');
const requirableFile = join(tmpdir.path, 'requirable.js');
const outputFile = join(tmpdir.path, process.platform === 'win32' ? 'sea.exe' : 'sea');

tmpdir.refresh();

writeFileSync(requirableFile, `
module.exports = {
  hello: 'world',
};
`);

let commonPathForSea = join(__dirname, '..', 'common');
if (process.platform === 'win32') {
  // Otherwise, the double backslashes get replaced with single backslashes in
  // the generated file.
  commonPathForSea = commonPathForSea.replace(/\\/g, '\\\\');
}

writeFileSync(inputFile, `
// Although, require('../common') works locally, that couldn't be used here
// because we set NODE_TEST_DIR=/Users/iojs/node-tmp on Jenkins CI.
const { expectWarning } = require('${commonPathForSea}');

expectWarning('ExperimentalWarning',
              'Single executable application is an experimental feature and ' +
              'might change at any time');

const { deepStrictEqual, strictEqual } = require('assert');
const { dirname } = require('path');

deepStrictEqual(process.argv, [process.execPath, process.execPath, '-a', '--b=c', 'd']);

strictEqual(__filename, process.execPath);
strictEqual(__dirname, dirname(process.execPath));
strictEqual(module.exports, exports);
strictEqual(require.main, module);

const requirable = require('./requirable.js');
deepStrictEqual(requirable, {
  hello: 'world',
});

console.log('Hello, world!');
`);
copyFileSync(process.execPath, outputFile);
const postjectFile = join(__dirname, '..', 'fixtures', 'postject-copy', 'node_modules', 'postject', 'dist', 'cli.js');
let postjectCommand = `${process.execPath} ${postjectFile} ${outputFile} NODE_JS_CODE ${inputFile} --sentinel-fuse NODE_JS_FUSE_fce680ab2cc467b6e072b8b5df1996b2`;
if (process.platform === 'darwin') {
  postjectCommand += ' --macho-segment-name NODE_JS';
}
execSync(postjectCommand);

if (process.platform === 'darwin') {
  execSync(`codesign --sign - ${outputFile}`);
  execSync(`codesign --verify ${outputFile}`);
} else if (process.platform === 'win32') {
  let signtoolFound = false;
  try {
    execSync('where signtool');
    signtoolFound = true;
  } catch (err) {
    console.log(err.message);
  }
  if (signtoolFound) {
    let certificatesFound = false;
    try {
      execSync(`signtool sign /fd SHA256 ${outputFile}`);
      certificatesFound = true;
    } catch (err) {
      if (!/SignTool Error: No certificates were found that met all the given criteria/.test(err)) {
        throw err;
      }
    }
    if (certificatesFound) {
      execSync(`signtool verify /pa SHA256 ${outputFile}`);
    }
  }
}

const singleExecutableApplicationOutput = execSync(`${outputFile} -a --b=c d`);
strictEqual(singleExecutableApplicationOutput.toString(), 'Hello, world!\n');
