'use strict';

const common = require('../common');
const assert = require('assert');
const execSync = require('child_process').execSync;
const fs = require('fs');
const path = require('path');

common.refreshTmpDir();
const filename = path.join(__dirname, '..', 'fixtures', 'baz.js');
const out = path.join(common.tmpDir, 'js.out');
const bin = process.execPath;
const input = `require('${filename}'); console.log('PASS');`;
const cmd = common.isWindows ?
  `echo "${input}" | ${bin} > ${out} 2>&1; more ${out}` :
  `echo "${input}" | ${bin} &> ${out}; cat ${out}`;

const result = execSync(cmd).toString();
assert.strictEqual(result.trim(), 'PASS');
fs.unlinkSync(out);
