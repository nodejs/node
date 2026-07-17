'use strict';

const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

if (process.argv[2] !== 'child') {
  // Expected error not emitted.
  {
    const child = spawn(
      process.execPath, [__filename, 'child', 0], { encoding: 'utf8' }
    );
    child.on('exit', common.mustCall((status) => {
      assert.notStrictEqual(status, 0);
    }));
  }

  // Expected error emitted.
  {
    const child = spawn(
      process.execPath, [__filename, 'child', 1], { encoding: 'utf8' }
    );
    child.on('exit', common.mustCall((status) => {
      assert.strictEqual(status, 0);
    }));
  }

  // Expected error emitted too many times.
  {
    const child = spawn(
      process.execPath, [__filename, 'child', 2], { encoding: 'utf8' }
    );
    child.stderr.setEncoding('utf8');

    let stderr = '';
    child.stderr.on('data', (data) => {
      stderr += data;
    });
    child.stderr.on('end', common.mustCall(() => {
      assert.match(stderr, /Unexpected extra warning received/);
    }));
    child.on('exit', common.mustCall((status) => {
      assert.notStrictEqual(status, 0);
    }));
  }
} else {
  const iterations = +process.argv[3];
  common.expectWarning('fhqwhgads', 'fhqwhgads', 'fhqwhgads');
  for (let i = 0; i < iterations; i++) {
    process.emitWarning('fhqwhgads', 'fhqwhgads', 'fhqwhgads');
  }
}
