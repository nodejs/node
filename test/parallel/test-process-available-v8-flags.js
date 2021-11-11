'use strict';

const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

// Assert legit flags are allowed, and bogus flags are disallowed
{
  const goodFlags = [
    '--single_threaded_gc',
    '--single-threaded-gc',
    'single-threaded-gc',
    '--single_threaded-gc',
    'single_threaded-gc',
    '--interrupt-budget=100',
    '--interrupt-budget=-=xX_nodejs_Xx=-',
  ];

  const badFlags = [
    'SINGLE-THREADED-GC',
    '--SINGLE-THREADED-GC',
    '--cheeseburgers',
  ];

  goodFlags.forEach((flag) => {
    assert.strictEqual(
      process.availableV8Flags.has(flag),
      true,
      `flag should be in set: ${flag}`
    );
  });

  badFlags.forEach((flag) => {
    assert.strictEqual(
      process.availableV8Flags.has(flag),
      false,
      `flag should not be in set: ${flag}`
    );
  });
}

// Assert all flags begin with dashes
{
  process.availableV8Flags.forEach((flag) => {
    assert.match(flag, /^--[a-zA-Z0-9._-]+$/);
  });
}

// Assert immutability of process.availableV8Flags
{
  assert.strictEqual(Object.isFrozen(process.availableV8Flags),
                     true);

  process.availableV8Flags.add('foo');
  assert.strictEqual(process.availableV8Flags.has('foo'), false);
  Set.prototype.add.call(process.availableV8Flags, 'foo');
  assert.strictEqual(process.availableV8Flags.has('foo'), false);

  const thisArg = {};
  process.availableV8Flags.forEach(
    common.mustCallAtLeast(function(flag, _, set) {
      assert.notStrictEqual(flag, 'foo');
      assert.strictEqual(this, thisArg);
      assert.strictEqual(set, process.availableV8Flags);
    }),
    thisArg
  );

  for (const flag of process.availableV8Flags.keys()) {
    assert.notStrictEqual(flag, 'foo');
  }
  for (const flag of process.availableV8Flags.values()) {
    assert.notStrictEqual(flag, 'foo');
  }
  for (const flag of process.availableV8Flags) {
    assert.notStrictEqual(flag, 'foo');
  }
  for (const [flag] of process.availableV8Flags.entries()) {
    assert.notStrictEqual(flag, 'foo');
  }

  const size = process.availableV8Flags.size;

  process.availableV8Flags.clear();
  assert.strictEqual(process.availableV8Flags.size, size);
  Set.prototype.clear.call(process.availableV8Flags);
  assert.strictEqual(process.availableV8Flags.size, size);

  process.availableV8Flags.delete('--single_threaded_gc');
  assert.strictEqual(process.availableV8Flags.size, size);
  Set.prototype.delete.call(process.availableV8Flags, '--single_threaded_gc');
  assert.strictEqual(process.availableV8Flags.size, size);
}

// Output of `node --v8-options` should match contents of availableV8Flags.
{
  let result = '';
  const command = spawn(process.execPath, ['--v8-options']);
  command.stdout.on('data', common.mustCallAtLeast((data) => {
    result += data.toString();
  }));
  command.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 0);
    result = result.replace(/^.*Options:/s, '');
    const cliOptions = [...result.matchAll(/^ {2}(--[^ ]+)/gm)]
      .map((m) => m[1])
      .sort();
    const v8Options = [...process.availableV8Flags.values()].sort();
    assert.deepStrictEqual(v8Options, cliOptions);
  }));
}
