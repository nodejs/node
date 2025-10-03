'use strict';

require('../common');
const assert = require('assert');
const util = require('util');
const { spawnSync } = require('child_process');

// Test basic functionality
{
  const tests = [
    { args: [], expected: '\n' },
    { args: [''], expected: '\n' },
    { args: ['test'], expected: 'test\n' },
    { args: ['foo', 'bar'], expected: 'foo bar\n' },
    { args: [null], expected: 'null\n' },
    { args: [undefined], expected: 'undefined\n' },
    { args: [true], expected: 'true\n' },
    { args: [false], expected: 'false\n' },
    { args: [42], expected: '42\n' },
    { args: [3.14], expected: '3.14\n' },
    { args: [{ a: 1 }], expected: '{ a: 1 }\n' },
    { args: [[1, 2, 3]], expected: '[ 1, 2, 3 ]\n' },
    { args: ['Hello %s', 'World'], expected: 'Hello World\n' },
    { args: ['Number: %d', 42], expected: 'Number: 42\n' },
  ];

  tests.forEach(({ args, expected }) => {
    const argsString = args.map((a) => {
      // Handle undefined specially since JSON.stringify(undefined) === undefined
      if (a === undefined) return 'undefined';
      return JSON.stringify(a);
    }).join(', ');

    const result = spawnSync(process.execPath, [
      '-e',
      `require('util').log(${argsString})`,
    ], { encoding: 'utf8' });

    assert.strictEqual(result.stdout, expected,
                       `util.log(${args.join(', ')}) failed`);
  });
}

// Test that util.log uses util.format
{
  const result = spawnSync(process.execPath, [
    '-e',
    'require("util").log("test %s %d", "string", 123)',
  ], { encoding: 'utf8' });

  assert.strictEqual(result.stdout, 'test string 123\n');
}

// Test async buffering behavior (output redirected to pipe)
{
  const script = `
    const util = require('util');
    for (let i = 0; i < 10; i++) {
      util.log('Line', i);
    }
  `;

  const result = spawnSync(process.execPath, ['-e', script], {
    encoding: 'utf8',
  });

  const lines = result.stdout.trim().split('\n');
  assert.strictEqual(lines.length, 10);
  assert.strictEqual(lines[0], 'Line 0');
  assert.strictEqual(lines[9], 'Line 9');
}

// Test that util.log is a function
{
  assert.strictEqual(typeof util.log, 'function');
}

// Test beforeExit flush (buffered writes should be flushed)
{
  const script = `
    const util = require('util');
    // These will be buffered when output is redirected
    util.log('buffered1');
    util.log('buffered2');
    util.log('buffered3');
    // Process will exit naturally, triggering beforeExit flush
  `;

  const result = spawnSync(process.execPath, ['-e', script], {
    encoding: 'utf8',
  });

  const lines = result.stdout.trim().split('\n');
  assert.strictEqual(lines.length, 3);
  assert.strictEqual(lines[0], 'buffered1');
  assert.strictEqual(lines[1], 'buffered2');
  assert.strictEqual(lines[2], 'buffered3');
}

// Test edge cases
{
  // Symbol
  const result1 = spawnSync(process.execPath, [
    '-e',
    'require("util").log(Symbol("test"))',
  ], { encoding: 'utf8' });
  assert.match(result1.stdout, /Symbol\(test\)/);

  // BigInt
  const result2 = spawnSync(process.execPath, [
    '-e',
    'require("util").log(123n)',
  ], { encoding: 'utf8' });
  assert.match(result2.stdout, /123n/);

  // Complex object
  const result3 = spawnSync(process.execPath, [
    '-e',
    'require("util").log({ nested: { value: 1 } })',
  ], { encoding: 'utf8' });
  assert.match(result3.stdout, /nested/);
}
