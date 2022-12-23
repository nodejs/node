'use strict';
const common = require('../common');

if (!process.config.variables.single_executable_application)
  common.skip('Single Executable Application support has been disabled.');

if (process.config.variables.asan)
  common.skip('ASAN builds fail with a SEGV on unknown address due to ' +
    'a READ memory access from FindSingleExecutableCode');

if (process.platform === 'aix')
  common.skip('XCOFF binary format not supported.');

if (process.platform === 'freebsd')
  common.skip('Running the resultant binary fails with `Exec format error`.');

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
require('${commonPathForSea}');

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
execSync(`${process.execPath} ${postjectFile} ${outputFile} NODE_JS_CODE ${inputFile}`);

if (process.platform === 'darwin') {
  execSync(`codesign --sign - ${outputFile}`);
  execSync(`codesign --verify ${outputFile}`);
}
// TODO(RaisinTen): Test code signing on Windows.

const singleExecutableApplicationOutput = execSync(`${outputFile} -a --b=c d`);
strictEqual(singleExecutableApplicationOutput.toString(), 'Hello, world!\n');
