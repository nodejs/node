'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const child = cp.spawn(process.execPath, ['-i']);
let output = '';

child.stdout.setEncoding('utf8');
child.stdout.on('data', (data) => {
  output += data;
});

child.on('exit', common.mustCall(() => {
  const results = output.replace(/^> /mg, '').split('\n').slice(2);
  assert.deepStrictEqual(
    results,
    [
      '[ 42, 23 ]',
      '1',
      '[ 42, ... 1 more item ]',
      '',
    ]
  );
}));

child.stdin.write('[ 42, 23 ]\n');
child.stdin.write('util.inspect.replDefaults.maxArrayLength = 1\n');
child.stdin.write('[ 42, 23 ]\n');
child.stdin.end();
