'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');
const fixtures = require('../common/fixtures');

const wrong_script = fixtures.path('cert.pem');

const p = child_process.spawn(process.execPath, [
  '-e',
  'require(process.argv[1]);',
  wrong_script
]);

p.stdout.on('data', common.mustNotCall());

let output = '';

p.stderr.on('data', (data) => output += data);

p.stderr.on('end', common.mustCall(() => {
  assert(/BEGIN CERT/.test(output));
  assert(/^\s+\^/m.test(output));
  assert(/Invalid left-hand side expression in prefix operation/.test(output));
}));
