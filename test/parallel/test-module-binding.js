// Flags: --expose-internals
'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const { internalBinding } = require('internal/test/binding');
const { internalModuleReadJSON } = internalBinding('fs');
const { readFileSync } = require('fs');
const { deepEqual, strictEqual } = require('assert');

strictEqual(internalModuleReadJSON('nosuchfile'), undefined);
deepEqual(internalModuleReadJSON(fixtures.path('empty.txt')), {});
strictEqual(internalModuleReadJSON(fixtures.path('empty-with-bom.txt')),
            undefined);
{
  const filename = fixtures.path('require-bin/package.json');
  strictEqual(internalModuleReadJSON(filename), readFileSync(filename, 'utf8'));
}
