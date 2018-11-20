'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const child = cp.spawn(process.execPath, ['--interactive']);
const fixtures = require('../common/fixtures');
const fixture = fixtures.path('is-object.js').replace(/\\/g, '/');
let output = '';

child.stdout.setEncoding('utf8');
child.stdout.on('data', (data) => {
  output += data;
});

child.on('exit', common.mustCall(() => {
  const results = output.replace(/^> /mg, '').split('\n');
  assert.deepStrictEqual(results, ['undefined', 'true', 'true', '']);
}));

child.stdin.write('const isObject = (obj) => obj.constructor === Object;\n');
child.stdin.write('isObject({});\n');
child.stdin.write(`require('${fixture}').isObject({});\n`);
child.stdin.write('.exit');
child.stdin.end();
