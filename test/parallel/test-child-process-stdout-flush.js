'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const spawn = require('child_process').spawn;
const sub = path.join(common.fixturesDir, 'print-chars.js');

const n = 500000;

const child = spawn(process.argv[0], [sub, n]);

let count = 0;

child.stderr.setEncoding('utf8');
child.stderr.on('data', function(data) {
  console.log('parent stderr: ' + data);
  assert.ok(false);
});

child.stdout.setEncoding('utf8');
child.stdout.on('data', function(data) {
  count += data.length;
  console.log(count);
});

child.on('close', function(data) {
  assert.equal(n, count);
  console.log('okay');
});
