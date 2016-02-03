'use strict';
const common = require('../common');

const spawn = require('child_process').spawn;
const path = require('path');
const assert = require('assert');


const fixture = path.join(
  common.fixturesDir,
  'child-process-stdout-tty-functions.js'
);

const proc = spawn(process.execPath, [fixture], { stdio: 'pipe' });
proc.stderr.setEncoding('utf8');

let stderr = '';

proc.stderr.on('data', (data) => stderr += data);

process.on('exit', (code) => {
  assert.equal(code, 0, 'the program should exit cleanly');
  assert.equal(stderr, '', stderr);
});
