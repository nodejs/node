'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');
const fixtures = require('../common/fixtures');

const wrong_script = fixtures.path('keys/rsa_cert.crt');

const p = child_process.spawn(process.execPath, [
  '-e',
  'require(process.argv[1]);',
  wrong_script,
]);

p.stdout.on('data', common.mustNotCall());

let output = '';

p.stderr.on('data', (data) => output += data);

p.stderr.on('end', common.mustCall(() => {
  assert.match(output, /BEGIN CERT/);
  assert.match(output, /^\s+\^/m);
  assert.match(output, /Invalid left-hand side expression in prefix operation/);
}));
