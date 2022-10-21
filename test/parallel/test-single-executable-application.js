'use strict';
const common = require('../common');

if (!process.config.variables.single_executable_application)
  common.skip('Single Executable Application support has been disabled.');

if (process.config.variables.asan)
  common.skip('ASAN builds fail with a SEGV on unknown address due to ' +
    'a READ memory access from FindSingleExecutableCode');

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

writeFileSync(inputFile, `
require('../common');

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
if (process.platform === 'win32') {
  execSync(`test\\fixtures\\postject-copy\\node_modules\\.bin\\postject.cmd ${outputFile} NODE_JS_CODE ${inputFile}`);
} else {
  execSync(`test/fixtures/postject-copy/node_modules/.bin/postject ${outputFile} NODE_JS_CODE ${inputFile}`);
}

// Verifying code signing using a self-signed certificate.
if (process.platform === 'darwin') {
  let codesignFound = false;
  try {
    execSync('command -v codesign');
    codesignFound = true;
  } catch (err) {
    console.log(err.message);
  }
  if (codesignFound) {
    execSync(`codesign --sign - ${outputFile}`);
    execSync(`codesign --verify ${outputFile}`);
  }
} else if (process.platform === 'win32') {
  let signtoolFound = false;
  try {
    execSync('where signtool');
    signtoolFound = true;
  } catch (err) {
    console.log(err.message);
  }
  if (signtoolFound) {
    execSync(`signtool sign /fd SHA256 ${outputFile}`);
    execSync(`signtool verify /pa SHA256 ${outputFile}`);
  }
}

const singleExecutableApplicationOutput = execSync(`${outputFile} -a --b=c d`);
strictEqual(singleExecutableApplicationOutput.toString(), 'Hello, world!\n');
