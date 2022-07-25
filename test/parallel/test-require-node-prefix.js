'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');

const errUnknownBuiltinModuleRE = /^No such built-in module: /u;

// For direct use of require expressions inside of CJS modules,
// all kinds of specifiers should work without issue.
{
  assert.strictEqual(require('fs'), fs);
  assert.strictEqual(require('node:fs'), fs);

  assert.throws(
    () => require('node:unknown'),
    {
      code: 'ERR_UNKNOWN_BUILTIN_MODULE',
      message: errUnknownBuiltinModuleRE,
    },
  );

  assert.throws(
    () => require('node:internal/test/binding'),
    {
      code: 'ERR_UNKNOWN_BUILTIN_MODULE',
      message: errUnknownBuiltinModuleRE,
    },
  );
}

// `node:`-prefixed `require(...)` calls bypass the require cache:
{
  const fakeModule = {};

  require.cache.fs = { exports: fakeModule };

  assert.strictEqual(require('fs'), fakeModule);
  assert.strictEqual(require('node:fs'), fs);

  delete require.cache.fs;
}
