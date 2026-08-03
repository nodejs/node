'use strict';
const common = require('../common');
const assert = require('assert');

const expected = '--option-to-be-seen-on-child';

const { spawn } = require('child_process');
const child = spawn(process.execPath, ['-', expected], { stdio: 'pipe' });

child.stdin.end('console.log(process.argv[2])');

let actual = '';
child.stdout.setEncoding('utf8');
child.stdout.on('data', (chunk) => actual += chunk);
child.stdout.on('end', common.mustCall(() => {
  assert.strictEqual(actual.trim(), expected);
}));
