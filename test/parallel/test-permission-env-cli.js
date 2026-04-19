// Flags: --permission --allow-fs-read=* --allow-env=HOME --allow-env=PATH
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');

// Guarantee the initial state
{
  assert.ok(!process.permission.has('env'));
}

// Allowed env vars should be accessible
{
  // Reading allowed env vars should not throw
  const home = process.env.HOME;
  const path = process.env.PATH;
  assert.ok(process.permission.has('env', 'HOME'));
  assert.ok(process.permission.has('env', 'PATH'));
}

// Disallowed env vars should return undefined (silently denied)
{
  assert.strictEqual(process.env.SECRET_KEY, undefined);
}

// Setting a disallowed env var should throw
{
  assert.throws(() => {
    process.env.NEW_VAR = 'value';
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'EnvVar',
    resource: 'NEW_VAR',
  }));
}

// Deleting a disallowed env var should throw
{
  assert.throws(() => {
    delete process.env.SECRET_KEY;
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'EnvVar',
    resource: 'SECRET_KEY',
  }));
}

// Querying a disallowed env var should return false (not found)
{
  assert.strictEqual('SECRET_KEY' in process.env, false);
}

// Enumerating should only return allowed env vars
{
  const keys = Object.keys(process.env);
  for (const key of keys) {
    assert.ok(
      key === 'HOME' || key === 'PATH',
      `Unexpected env var in enumeration: ${key}`
    );
  }
}
