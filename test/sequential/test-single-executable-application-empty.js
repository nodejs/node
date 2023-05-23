'use strict';

require('../common');

const {
  injectAndCodeSign,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

// This tests the creation of a single executable application with an empty
// script.

const tmpdir = require('../common/tmpdir');
const { copyFileSync, writeFileSync, existsSync } = require('fs');
const { execFileSync } = require('child_process');
const { join } = require('path');
const assert = require('assert');

const configFile = join(tmpdir.path, 'sea-config.json');
const seaPrepBlob = join(tmpdir.path, 'sea-prep.blob');
const outputFile = join(tmpdir.path, process.platform === 'win32' ? 'sea.exe' : 'sea');

tmpdir.refresh();

writeFileSync(join(tmpdir.path, 'empty.js'), '', 'utf-8');
writeFileSync(configFile, `
{
  "main": "empty.js",
  "output": "sea-prep.blob"
}
`);

execFileSync(process.execPath, ['--experimental-sea-config', 'sea-config.json'], {
  cwd: tmpdir.path
});

assert(existsSync(seaPrepBlob));

copyFileSync(process.execPath, outputFile);
injectAndCodeSign(outputFile, seaPrepBlob);

execFileSync(outputFile);
