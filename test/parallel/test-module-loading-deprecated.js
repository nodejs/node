// Flags: --pending-deprecation

'use strict';

const common = require('../common');
const assert = require('assert');

common.expectWarning('DeprecationWarning', {
  DEP0XXX: /^Invalid 'main' field in '.+main[/\\]package\.json' of 'doesnotexist\.js'\..+module author/
});

assert.strictEqual(require('../fixtures/packages/missing-main').ok, 'ok');
