// Flags: --permission --allow-fs-read=* --allow-env=HOME --allow-env=PATH
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');

{
  assert.ok(!process.permission.has('env'));
}

{
  assert.ok(process.env.HOME !== undefined);
  assert.ok(process.env.PATH !== undefined);
  assert.ok(process.permission.has('env', 'HOME'));
  assert.ok(process.permission.has('env', 'PATH'));
}

{
  assert.strictEqual(process.env.SECRET_KEY, undefined);
}

{
  assert.throws(() => {
    process.env.NEW_VAR = 'value';
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'EnvVar',
    resource: 'NEW_VAR',
  }));
}

{
  assert.throws(() => {
    delete process.env.SECRET_KEY;
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'EnvVar',
    resource: 'SECRET_KEY',
  }));
}

{
  assert.strictEqual('SECRET_KEY' in process.env, false);
}

{
  const keys = Object.keys(process.env);
  for (const key of keys) {
    assert.ok(
      key === 'HOME' || key === 'PATH',
      `Unexpected env var in enumeration: ${key}`
    );
  }
}

{
  const keys = [];
  for (const key in process.env) {
    keys.push(key);
  }
  for (const key of keys) {
    assert.ok(
      key === 'HOME' || key === 'PATH',
      `Unexpected env var in for...in: ${key}`
    );
  }
}

{
  const copy = Object.assign({}, process.env);
  const keys = Object.keys(copy);
  for (const key of keys) {
    assert.ok(
      key === 'HOME' || key === 'PATH',
      `Unexpected env var in Object.assign: ${key}`
    );
  }
}

{
  const parsed = JSON.parse(JSON.stringify(process.env));
  const keys = Object.keys(parsed);
  for (const key of keys) {
    assert.ok(
      key === 'HOME' || key === 'PATH',
      `Unexpected env var in JSON.stringify: ${key}`
    );
  }
}
