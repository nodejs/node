'use strict';
const { expectWarning } = require('../common');
const assert = require('assert');

expectWarning('Warning', [
  [
    'Attempting to require and ESModule. Replace `require` with `import`',
    'WARN_REQUIRE_ESM',
  ],
  [
    'Attempting to require and ESModule. Replace `require` with `import`',
    'WARN_REQUIRE_ESM',
  ],
]);

require('../fixtures/es-modules/test-esm-ok.mjs').then((module) => {
  assert.ok(module.default);
});

require('../fixtures/es-modules/package-type-module').then((module) => {
  assert.strictEqual(module.default, 'package-type-module');
});
