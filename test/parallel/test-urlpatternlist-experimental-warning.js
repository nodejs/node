'use strict';

// URLPatternList emits an ExperimentalWarning once, when it is first
// constructed. Loading node:url, reading the export, importing the ESM
// facade, which reads every export, or calling the constructor without `new`
// does not emit it.

const common = require('../common');
const assert = require('assert');

let constructed = false;
process.on('warning', common.mustCall((warning) => {
  assert.strictEqual(constructed, true);
  assert.strictEqual(warning.name, 'ExperimentalWarning');
  assert.strictEqual(
    warning.message,
    'URLPatternList is an experimental feature and might change at any time');
}));

const { URLPattern, URLPatternList } = require('url');

import('node:url').then(common.mustCall((namespace) => {
  assert.strictEqual(namespace.URLPatternList, URLPatternList);
  assert.throws(() => URLPatternList([new URLPattern()]), {
    code: 'ERR_CONSTRUCT_CALL_REQUIRED',
  });
  setImmediate(common.mustCall(() => {
    constructed = true;
    assert.throws(() => new URLPatternList([]), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
    new URLPatternList([new URLPattern()]);
    new URLPatternList([new URLPattern()]);
  }));
}));
