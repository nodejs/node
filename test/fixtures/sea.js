const { Module: { createRequire } } = require('module');
const createdRequire = createRequire(__filename);

// Although, require('../common') works locally, that couldn't be used here
// because we set NODE_TEST_DIR=/Users/iojs/node-tmp on Jenkins CI.
const { expectWarning } = createdRequire(process.env.COMMON_DIRECTORY);

expectWarning('ExperimentalWarning',
              'Single executable application is an experimental feature and ' +
              'might change at any time');

// Should be possible to require core modules that optionally require the
// "node:" scheme.
const { deepStrictEqual, strictEqual, throws } = require('assert');
const { dirname } = require('node:path');

// Should be possible to require a core module that requires using the "node:"
// scheme.
{
  const { test } = require('node:test');
  strictEqual(typeof test, 'function');
}

// Should not be possible to require a core module without the "node:" scheme if
// it requires using the "node:" scheme.
throws(() => require('test'), {
  code: 'ERR_UNKNOWN_BUILTIN_MODULE',
});

deepStrictEqual(process.argv, [process.execPath, process.execPath, '-a', '--b=c', 'd']);

strictEqual(require.cache, undefined);
strictEqual(require.extensions, undefined);
strictEqual(require.main, module);
strictEqual(require.resolve, undefined);

strictEqual(__filename, process.execPath);
strictEqual(__dirname, dirname(process.execPath));
strictEqual(module.exports, exports);

throws(() => require('./requirable.js'), {
  code: 'ERR_UNKNOWN_BUILTIN_MODULE',
});

const requirable = createdRequire('./requirable.js');
deepStrictEqual(requirable, {
  hello: 'world',
});

console.log('Hello, world! 😊');
