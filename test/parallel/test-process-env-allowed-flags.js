'use strict';

require('../common');
const assert = require('assert');

// Assert legit flags are allowed, and bogus flags are disallowed
{
  const goodFlags = [
    '--perf_basic_prof',
    '--perf-basic-prof',
    'perf-basic-prof',
    '--perf_basic-prof',
    'perf_basic-prof',
    'perf_basic_prof',
    '-r',
    'r',
    '--stack-trace-limit=100',
    '--stack-trace-limit=-=xX_nodejs_Xx=-',
  ].concat(process.features.inspector ? [
    '--inspect-brk',
    'inspect-brk',
    '--inspect_brk',
  ] : []);

  const badFlags = [
    'INSPECT-BRK',
    '--INSPECT-BRK',
    '--r',
    '-R',
    '---inspect-brk',
    '--cheeseburgers'
  ];

  goodFlags.forEach((flag) => {
    assert.strictEqual(
      process.allowedNodeEnvironmentFlags.has(flag),
      true,
      `flag should be in set: ${flag}`
    );
  });

  badFlags.forEach((flag) => {
    assert.strictEqual(
      process.allowedNodeEnvironmentFlags.has(flag),
      false,
      `flag should not be in set: ${flag}`
    );
  });
}

// Assert all "canonical" flags begin with dash(es)
{
  process.allowedNodeEnvironmentFlags.forEach((flag) => {
    assert(/^--?[a-z0-9._-]+$/.test(flag),
           `Unexpected format for flag ${flag}`);
  });
}

// Assert immutability of process.allowedNodeEnvironmentFlags
{
  assert.strictEqual(Object.isFrozen(process.allowedNodeEnvironmentFlags),
                     true);

  process.allowedNodeEnvironmentFlags.add('foo');
  assert.strictEqual(process.allowedNodeEnvironmentFlags.has('foo'), false);
  process.allowedNodeEnvironmentFlags.forEach((flag) => {
    assert.strictEqual(flag === 'foo', false);
  });

  process.allowedNodeEnvironmentFlags.clear();
  assert.strictEqual(process.allowedNodeEnvironmentFlags.size > 0, true);

  const size = process.allowedNodeEnvironmentFlags.size;
  process.allowedNodeEnvironmentFlags.delete('-r');
  assert.strictEqual(process.allowedNodeEnvironmentFlags.size, size);
}
