'use strict';

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const node = process.execPath;

if (process.argv[2] === 'child') {
  const rejectPromise = () => {
    return new Promise((resolve, reject) => {
      setTimeout(() => reject(new Error('Reject Promise')), 100);
    });
  };
  rejectPromise();
} else {
  run('--throw-unhandled-rejection');
}

function run(flags) {
  const args = [__filename, 'child'];
  if (flags)
    args.unshift(flags);

  const child = spawn(node, args);
  let message = '';
  child.stderr.on('data', (data) => {
    message += data;
  });
  child.stderr.on('end', common.mustCall(() => {
    assert(message.match(/Reject Promise/));
  }));
  child.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 1);
  }));
}

